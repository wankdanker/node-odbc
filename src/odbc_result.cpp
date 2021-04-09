/*
  Copyright (c) 2013, Dan VerWeire<dverweire@gmail.com>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <string.h>
#include <v8.h>
#include <node.h>
#include <node_version.h>
#include <time.h>
#include <uv.h>

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_result.h"
#include "odbc_statement.h"

using namespace v8;
using namespace node;

Nan::Persistent<Function> ODBCResult::constructor;
Nan::Persistent<String> ODBCResult::OPTION_FETCH_MODE;

void ODBCResult::Init(Local<Object> exports) {
  DEBUG_PRINTF("ODBCResult::Init\n");
  Nan::HandleScope scope;

  Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);

  // Constructor Template
  t->SetClassName(Nan::New("ODBCResult").ToLocalChecked());

  // Reserve space for one Handle<Value>
  Local<ObjectTemplate> i = t->InstanceTemplate();
  i->SetInternalFieldCount(1);
  
  // Prototype Methods  
  Nan::SetPrototypeMethod(t, "fetchAll", FetchAll);
  Nan::SetPrototypeMethod(t, "fetch", Fetch);

  Nan::SetPrototypeMethod(t, "moreResultsSync", MoreResultsSync);
  Nan::SetPrototypeMethod(t, "closeSync", CloseSync);
  Nan::SetPrototypeMethod(t, "fetchSync", FetchSync);
  Nan::SetPrototypeMethod(t, "fetchAllSync", FetchAllSync);
  Nan::SetPrototypeMethod(t, "getColumnNamesSync", GetColumnNamesSync);
  Nan::SetPrototypeMethod(t, "getRowCountSync", GetRowCountSync);

  // Properties
  OPTION_FETCH_MODE.Reset(Nan::New("fetchMode").ToLocalChecked());
  Nan::SetAccessor(i, Nan::New("fetchMode").ToLocalChecked(), FetchModeGetter, FetchModeSetter);
  
  // Attach the Database Constructor to the target object
  constructor.Reset(Nan::GetFunction(t).ToLocalChecked());
  Nan::Set(exports,
    Nan::New("ODBCResult").ToLocalChecked(),
    Nan::GetFunction(t).ToLocalChecked());
}

ODBCResult::~ODBCResult() {
  DEBUG_PRINTF("ODBCResult::~ODBCResult\n");
  //DEBUG_PRINTF("ODBCResult::~ODBCResult m_hSTMT=%x\n", m_hSTMT);
  this->Free();
}

void ODBCResult::Free() {
  DEBUG_PRINTF("ODBCResult::Free\n");
  //DEBUG_PRINTF("ODBCResult::Free m_hSTMT=%X m_canFreeHandle=%X\n", m_hSTMT, m_canFreeHandle);

  if (m_hSTMT && m_canFreeHandle) {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    SQLFreeHandle( SQL_HANDLE_STMT, m_hSTMT);
    
    m_hSTMT = NULL;
  
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
  
  if (bufferLength > 0) {
    bufferLength = 0;
    free(buffer);
  }
}

NAN_METHOD(ODBCResult::New) {
  DEBUG_PRINTF("ODBCResult::New\n");
  Nan::HandleScope scope;
  
  REQ_EXT_ARG(0, js_henv);
  REQ_EXT_ARG(1, js_hdbc);
  REQ_EXT_ARG(2, js_hstmt);
  REQ_EXT_ARG(3, js_canFreeHandle);
  
  HENV hENV = static_cast<HENV>(js_henv->Value());
  HDBC hDBC = static_cast<HDBC>(js_hdbc->Value());
  HSTMT hSTMT = static_cast<HSTMT>(js_hstmt->Value());
  bool* canFreeHandle = static_cast<bool *>(js_canFreeHandle->Value());
  
  //create a new OBCResult object
  ODBCResult* objODBCResult = new ODBCResult(hENV, hDBC, hSTMT, *canFreeHandle);
  
  DEBUG_PRINTF("ODBCResult::New\n");
  //DEBUG_PRINTF("ODBCResult::New m_hDBC=%X m_hDBC=%X m_hSTMT=%X canFreeHandle=%X\n",
  //  objODBCResult->m_hENV,
  //  objODBCResult->m_hDBC,
  //  objODBCResult->m_hSTMT,
  //  objODBCResult->m_canFreeHandle
  //);
  
  //free the pointer to canFreeHandle
  delete canFreeHandle;

  //specify the buffer length
  objODBCResult->bufferLength = MAX_VALUE_SIZE - 1;
  
  //initialze a buffer for this object
  objODBCResult->buffer = (uint16_t *) malloc(objODBCResult->bufferLength + 1);
  //TODO: make sure the malloc succeeded

  //set the initial colCount to 0
  objODBCResult->colCount = 0;

  //default fetchMode to FETCH_OBJECT
  objODBCResult->m_fetchMode = FETCH_OBJECT;
  
  objODBCResult->Wrap(info.Holder());
  
  info.GetReturnValue().Set(info.Holder());
}

NAN_GETTER(ODBCResult::FetchModeGetter) {
  Nan::HandleScope scope;

  ODBCResult *obj = Nan::ObjectWrap::Unwrap<ODBCResult>(info.Holder());

  info.GetReturnValue().Set(Nan::New(obj->m_fetchMode));
}

NAN_SETTER(ODBCResult::FetchModeSetter) {
  Nan::HandleScope scope;

  ODBCResult *obj = Nan::ObjectWrap::Unwrap<ODBCResult>(info.Holder());
  
  if (value->IsNumber()) {
    obj->m_fetchMode = Local<Int32>::Cast(value)->Value();
  }
}

/*
 * Fetch
 */

NAN_METHOD(ODBCResult::Fetch) {
  DEBUG_PRINTF("ODBCResult::Fetch\n");
  Nan::HandleScope scope;
  
  ODBCResult* objODBCResult = Nan::ObjectWrap::Unwrap<ODBCResult>(info.Holder());
  
  uv_work_t* work_req = (uv_work_t *) (calloc(1, sizeof(uv_work_t)));
  
  fetch_work_data* data = (fetch_work_data *) calloc(1, sizeof(fetch_work_data));
  
  Local<Function> cb;
   
  //set the fetch mode to the default of this instance
  data->fetchMode = objODBCResult->m_fetchMode;
  
  if (info.Length() == 1 && info[0]->IsFunction()) {
    cb = Local<Function>::Cast(info[0]);
  }
  else if (info.Length() == 2 && info[0]->IsObject() && info[1]->IsFunction()) {
    cb = Local<Function>::Cast(info[1]);  
    
    Local<Object> obj = Local<Object>::Cast(info[0]);
    
    Local<String> fetchModeKey = Nan::New<String>(OPTION_FETCH_MODE);
    if (Nan::Has(obj, fetchModeKey).FromJust() && Nan::Get(obj, fetchModeKey).ToLocalChecked()->IsInt32()) {
      data->fetchMode = Nan::To<int>(Nan::Get(obj, fetchModeKey).ToLocalChecked()).FromJust();
    }
  }
  else {
    return Nan::ThrowTypeError("ODBCResult::Fetch(): 1 or 2 arguments are required. The last argument must be a callback function.");
  }
  
  data->cb = new Nan::Callback(cb);
  
  data->isolate = info.GetIsolate();
  data->objResult = objODBCResult;
  work_req->data = data;
  
  uv_queue_work(
    uv_default_loop(), 
    work_req, 
    UV_Fetch, 
    (uv_after_work_cb)UV_AfterFetch);

  objODBCResult->Ref();

  info.GetReturnValue().Set(Nan::Undefined());
}

void ODBCResult::UV_Fetch(uv_work_t* work_req) {
  DEBUG_PRINTF("ODBCResult::UV_Fetch\n");
  
  fetch_work_data* data = (fetch_work_data *)(work_req->data);
  
  data->result = SQLFetch(data->objResult->m_hSTMT);
}

void ODBCResult::UV_AfterFetch(uv_work_t* work_req, int status) {
  DEBUG_PRINTF("ODBCResult::UV_AfterFetch\n");
  Nan::HandleScope scope;
  
  fetch_work_data* data = (fetch_work_data *)(work_req->data);
  
  SQLRETURN ret = data->result;
  //TODO: we should probably define this on the work data so we
  //don't have to keep creating it?
  Local<Object> objError;
  bool moreWork = true;
  bool error = false;
  
  if (data->objResult->colCount == 0) {
    data->objResult->columns = ODBC::GetColumns(
      data->objResult->m_hSTMT, 
      &data->objResult->colCount);
  }
  
  //check to see if the result has no columns
  if (data->objResult->colCount == 0) {
    //this means
    moreWork = false;
  }
  //check to see if there was an error
  else if (ret == SQL_ERROR)  {
    moreWork = false;
    error = true;
    
    objError = ODBC::GetSQLError(
      SQL_HANDLE_STMT, 
      data->objResult->m_hSTMT,
      (char *) "Error in ODBCResult::UV_AfterFetch");
  }
  //check to see if we are at the end of the recordset
  else if (ret == SQL_NO_DATA) {
    moreWork = false;
  }

  if (moreWork) {
    Local<Value> info[2];

    info[0] = Nan::Null();
    if (data->fetchMode == FETCH_ARRAY) {
      info[1] = ODBC::GetRecordArray(
        data->objResult->m_hSTMT,
        data->objResult->columns,
        &data->objResult->colCount,
        data->objResult->buffer,
        data->objResult->bufferLength,
        data->isolate);
    }
    else {
      info[1] = ODBC::GetRecordTuple(
        data->objResult->m_hSTMT,
        data->objResult->columns,
        &data->objResult->colCount,
        data->objResult->buffer,
        data->objResult->bufferLength,
        data->isolate);
    }

    Nan::TryCatch try_catch;

    data->cb->Call(2, info);
    delete data->cb;

    if (try_catch.HasCaught()) {
        Nan::FatalException(try_catch);
    }
  }
  else {
    ODBC::FreeColumns(data->objResult->columns, &data->objResult->colCount);
    
    Local<Value> info[2];
    
    //if there was an error, pass that as arg[0] otherwise Null
    if (error) {
      info[0] = objError;
    }
    else {
      info[0] = Nan::Null();
    }
    
    info[1] = Nan::Null();

    Nan::TryCatch try_catch;

    data->cb->Call(2, info);
    delete data->cb;

    if (try_catch.HasCaught()) {
        Nan::FatalException(try_catch);
    }
  }
  
  data->objResult->Unref();
  
  free(data);
  free(work_req);
  
  return;
}

/*
 * FetchSync
 */

NAN_METHOD(ODBCResult::FetchSync) {
  DEBUG_PRINTF("ODBCResult::FetchSync\n");
  Nan::HandleScope scope;
  
  ODBCResult* objResult = Nan::ObjectWrap::Unwrap<ODBCResult>(info.Holder());

  Local<Value> objError;
  bool moreWork = true;
  bool error = false;
  int fetchMode = objResult->m_fetchMode;
  
  if (info.Length() == 1 && info[0]->IsObject()) {
    Local<Object> obj = Nan::To<Object>(info[0]).ToLocalChecked();
    
    Local<String> fetchModeKey = Nan::New<String>(OPTION_FETCH_MODE);
    if (Nan::Has(obj, fetchModeKey).FromJust() && Nan::Get(obj, fetchModeKey).ToLocalChecked()->IsInt32()) {
      fetchMode = Nan::To<int>(Nan::Get(obj, fetchModeKey).ToLocalChecked()).FromJust();
    }
  }
  
  SQLRETURN ret = SQLFetch(objResult->m_hSTMT);

  if (objResult->colCount == 0) {
    objResult->columns = ODBC::GetColumns(
      objResult->m_hSTMT, 
      &objResult->colCount);
  }
  
  //check to see if the result has no columns
  if (objResult->colCount == 0) {
    moreWork = false;
  }
  //check to see if there was an error
  else if (ret == SQL_ERROR)  {
    moreWork = false;
    error = true;
    
    objError = ODBC::GetSQLError(
      SQL_HANDLE_STMT, 
      objResult->m_hSTMT,
      (char *) "Error in ODBCResult::UV_AfterFetch");
  }
  //check to see if we are at the end of the recordset
  else if (ret == SQL_NO_DATA) {
    moreWork = false;
  }

  if (moreWork) {
    Local<Value> data;
    
    if (fetchMode == FETCH_ARRAY) {
      data = ODBC::GetRecordArray(
        objResult->m_hSTMT,
        objResult->columns,
        &objResult->colCount,
        objResult->buffer,
        objResult->bufferLength,
        info.GetIsolate());
    }
    else {
      data = ODBC::GetRecordTuple(
        objResult->m_hSTMT,
        objResult->columns,
        &objResult->colCount,
        objResult->buffer,
        objResult->bufferLength,
        info.GetIsolate());
    }
    
    info.GetReturnValue().Set(data);
  }
  else {
    ODBC::FreeColumns(objResult->columns, &objResult->colCount);

    //if there was an error, pass that as arg[0] otherwise Null
    if (error) {
      Nan::ThrowError(objError);
      
      info.GetReturnValue().Set(Nan::Null());
    }
    else {
      info.GetReturnValue().Set(Nan::Null());
    }
  }
}

/*
 * FetchAll
 */

NAN_METHOD(ODBCResult::FetchAll) {
  DEBUG_PRINTF("ODBCResult::FetchAll\n");
  Nan::HandleScope scope;
  
  ODBCResult* objODBCResult = Nan::ObjectWrap::Unwrap<ODBCResult>(info.Holder());
  
  uv_work_t* work_req = (uv_work_t *) (calloc(1, sizeof(uv_work_t)));
  
  fetch_work_data* data = (fetch_work_data *) calloc(1, sizeof(fetch_work_data));
  
  Local<Function> cb;
  
  data->fetchMode = objODBCResult->m_fetchMode;
  
  if (info.Length() == 1 && info[0]->IsFunction()) {
    cb = Local<Function>::Cast(info[0]);
  }
  else if (info.Length() == 2 && info[0]->IsObject() && info[1]->IsFunction()) {
    cb = Local<Function>::Cast(info[1]);  
    
    Local<Object> obj = Nan::To<Object>(info[0]).ToLocalChecked();
    
    Local<String> fetchModeKey = Nan::New<String>(OPTION_FETCH_MODE);
    if (Nan::Has(obj, fetchModeKey).FromJust() && Nan::Get(obj, fetchModeKey).ToLocalChecked()->IsInt32()) {
      data->fetchMode = Nan::To<int>(Nan::Get(obj, fetchModeKey).ToLocalChecked()).FromJust();
    }
  }
  else {
    Nan::ThrowTypeError("ODBCResult::FetchAll(): 1 or 2 arguments are required. The last argument must be a callback function.");
  }
  
  data->rows.Reset(Nan::New<Array>());
  data->errorCount = 0;
  data->count = 0;
  data->objError.Reset(Nan::New<Object>());
  data->isolate = info.GetIsolate();
  data->cb = new Nan::Callback(cb);
  data->objResult = objODBCResult;
  
  work_req->data = data;
  
  uv_queue_work(uv_default_loop(),
    work_req, 
    UV_FetchAll, 
    (uv_after_work_cb)UV_AfterFetchAll);

  data->objResult->Ref();

  info.GetReturnValue().Set(Nan::Undefined());
}

void ODBCResult::UV_FetchAll(uv_work_t* work_req) {
  DEBUG_PRINTF("ODBCResult::UV_FetchAll\n");
  
  fetch_work_data* data = (fetch_work_data *)(work_req->data);
  
  data->result = SQLFetch(data->objResult->m_hSTMT);
 }

void ODBCResult::UV_AfterFetchAll(uv_work_t* work_req, int status) {
  DEBUG_PRINTF("ODBCResult::UV_AfterFetchAll\n");
  Nan::HandleScope scope;
  
  fetch_work_data* data = (fetch_work_data *)(work_req->data);
  
  ODBCResult* self = data->objResult->self();
  
  bool doMoreWork = true;
  
  if (self->colCount == 0) {
    self->columns = ODBC::GetColumns(self->m_hSTMT, &self->colCount);
  }
  
  //check to see if the result set has columns
  if (self->colCount == 0) {
    //this most likely means that the query was something like
    //'insert into ....'
    doMoreWork = false;
  }
  //check to see if there was an error
  else if (data->result == SQL_ERROR)  {
    data->errorCount++;

    //NanAssignPersistent(data->objError, ODBC::GetSQLError(
    data->objError.Reset(ODBC::GetSQLError(
        SQL_HANDLE_STMT, 
        self->m_hSTMT,
        (char *) "[node-odbc] Error in ODBCResult::UV_AfterFetchAll"
    ));
    
    doMoreWork = false;
  }
  //check to see if we are at the end of the recordset
  else if (data->result == SQL_NO_DATA) {
    doMoreWork = false;
  }
  else {
    Local<Array> rows = Nan::New(data->rows);
    if (data->fetchMode == FETCH_ARRAY) {
      Nan::Set(rows,
        Nan::New(data->count), 
        ODBC::GetRecordArray(
          self->m_hSTMT,
          self->columns,
          &self->colCount,
          self->buffer,
          self->bufferLength,
          data->isolate)
      );
    }
    else {
      Nan::Set(rows,
        Nan::New(data->count), 
        ODBC::GetRecordTuple(
          self->m_hSTMT,
          self->columns,
          &self->colCount,
          self->buffer,
          self->bufferLength,
          data->isolate)
      );
    }
    data->count++;
  }
  
  if (doMoreWork) {
    //Go back to the thread pool and fetch more data!
    uv_queue_work(
      uv_default_loop(),
      work_req, 
      UV_FetchAll, 
      (uv_after_work_cb)UV_AfterFetchAll);
  }
  else {
    ODBC::FreeColumns(self->columns, &self->colCount);
    
    Local<Value> info[2];
    
    if (data->errorCount > 0) {
      info[0] = Nan::New(data->objError);
    }
    else {
      info[0] = Nan::Null();
    }
    
    info[1] = Nan::New(data->rows);

    Nan::TryCatch try_catch;

    data->cb->Call(2, info);
    delete data->cb;
    data->rows.Reset();
    data->objError.Reset();

    if (try_catch.HasCaught()) {
        Nan::FatalException(try_catch);
    }

    free(data);
    free(work_req);

    self->Unref(); 
  }
}

/*
 * FetchAllSync
 */

NAN_METHOD(ODBCResult::FetchAllSync) {
  DEBUG_PRINTF("ODBCResult::FetchAllSync\n");
  Nan::HandleScope scope;
  
  ODBCResult* self = Nan::ObjectWrap::Unwrap<ODBCResult>(info.Holder());
  
  Local<Value> objError = Nan::New<Object>();
  
  SQLRETURN ret;
  int count = 0;
  int errorCount = 0;
  int fetchMode = self->m_fetchMode;

  if (info.Length() == 1 && info[0]->IsObject()) {
    Local<Object> obj = Nan::To<Object>(info[0]).ToLocalChecked();
    
    Local<String> fetchModeKey = Nan::New<String>(OPTION_FETCH_MODE);
    if (Nan::Has(obj, fetchModeKey).FromJust() && Nan::Get(obj, fetchModeKey).ToLocalChecked()->IsInt32()) {
      fetchMode = Nan::To<int>(Nan::Get(obj, fetchModeKey).ToLocalChecked()).FromJust();
    }
  }
  
  if (self->colCount == 0) {
    self->columns = ODBC::GetColumns(self->m_hSTMT, &self->colCount);
  }
  
  Local<Array> rows = Nan::New<Array>();
  
  //Only loop through the recordset if there are columns
  if (self->colCount > 0) {
    //loop through all records
    while (true) {
      ret = SQLFetch(self->m_hSTMT);
      
      //check to see if there was an error
      if (ret == SQL_ERROR)  {
        errorCount++;
        
        objError = ODBC::GetSQLError(
          SQL_HANDLE_STMT, 
          self->m_hSTMT,
          (char *) "[node-odbc] Error in ODBCResult::UV_AfterFetchAll; probably"
            " your query did not have a result set."
        );
        
        break;
      }
      
      //check to see if we are at the end of the recordset
      if (ret == SQL_NO_DATA) {
        ODBC::FreeColumns(self->columns, &self->colCount);
        
        break;
      }

      if (fetchMode == FETCH_ARRAY) {
        Nan::Set(rows,
          Nan::New(count), 
          ODBC::GetRecordArray(
            self->m_hSTMT,
            self->columns,
            &self->colCount,
            self->buffer,
            self->bufferLength,
            info.GetIsolate())
        );
      }
      else {
        Nan::Set(rows,
          Nan::New(count), 
          ODBC::GetRecordTuple(
            self->m_hSTMT,
            self->columns,
            &self->colCount,
            self->buffer,
            self->bufferLength,
            info.GetIsolate())
        );
      }
      count++;
    }
  }
  else {
    ODBC::FreeColumns(self->columns, &self->colCount);
  }
  
  //throw the error object if there were errors
  if (errorCount > 0) {
    Nan::ThrowError(objError);
  }
  
  info.GetReturnValue().Set(rows);
}

/*
 * CloseSync
 * 
 */

NAN_METHOD(ODBCResult::CloseSync) {
  DEBUG_PRINTF("ODBCResult::CloseSync\n");
  Nan::HandleScope scope;
  
  OPT_INT_ARG(0, closeOption, SQL_DESTROY);
  
  ODBCResult* result = Nan::ObjectWrap::Unwrap<ODBCResult>(info.Holder());
 
  DEBUG_PRINTF("ODBCResult::CloseSync closeOption=%i m_canFreeHandle=%i\n", 
               closeOption, result->m_canFreeHandle);
  
  if (closeOption == SQL_DESTROY && result->m_canFreeHandle) {
    result->Free();
  }
  else if (closeOption == SQL_DESTROY && !result->m_canFreeHandle) {
    //We technically can't free the handle so, we'll SQL_CLOSE
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    SQLFreeStmt(result->m_hSTMT, SQL_CLOSE);
  
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
  else {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    SQLFreeStmt(result->m_hSTMT, closeOption);
  
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
  
  info.GetReturnValue().Set(Nan::True());
}

NAN_METHOD(ODBCResult::MoreResultsSync) {
  DEBUG_PRINTF("ODBCResult::MoreResultsSync\n");
  Nan::HandleScope scope;
  
  ODBCResult* result = Nan::ObjectWrap::Unwrap<ODBCResult>(info.Holder());
  
  SQLRETURN ret = SQLMoreResults(result->m_hSTMT);

  if (ret == SQL_ERROR) {
    Local<Value> objError = ODBC::GetSQLError(
    	SQL_HANDLE_STMT, 
	    result->m_hSTMT, 
	    (char *)"[node-odbc] Error in ODBCResult::MoreResultsSync"
    );

    Nan::ThrowError(objError);
  }

  info.GetReturnValue().Set(SQL_SUCCEEDED(ret) || ret == SQL_ERROR ? Nan::True() : Nan::False());
}

/*
 * GetColumnNamesSync
 */

NAN_METHOD(ODBCResult::GetColumnNamesSync) {
  DEBUG_PRINTF("ODBCResult::GetColumnNamesSync\n");
  Nan::HandleScope scope;
  
  ODBCResult* self = Nan::ObjectWrap::Unwrap<ODBCResult>(info.Holder());
  
  Local<Array> cols = Nan::New<Array>();
  
  if (self->colCount == 0) {
    self->columns = ODBC::GetColumns(self->m_hSTMT, &self->colCount);
  }
  
  for (int i = 0; i < self->colCount; i++) {
#ifdef UNICODE
    Nan::Set(cols,
      Nan::New(i),
      Nan::New((uint16_t*) self->columns[i].name).ToLocalChecked());
#else
    Nan::Set(cols,
      Nan::New(i),
      Nan::New((char *) self->columns[i].name).ToLocalChecked());
#endif

  }
    
  info.GetReturnValue().Set(cols);
}

/*
 * GetRowCountSync
 */

NAN_METHOD(ODBCResult::GetRowCountSync) {
  DEBUG_PRINTF("ODBCResult::GetRowCountSync\n");
  Nan::HandleScope scope;

  ODBCResult* self = Nan::ObjectWrap::Unwrap<ODBCResult>(info.Holder());

  SQLLEN rowCount = 0;

  SQLRETURN ret = SQLRowCount(self->m_hSTMT, &rowCount);

  if (!SQL_SUCCEEDED(ret)) {
    rowCount = 0;
  }

  info.GetReturnValue().Set(Nan::New<Number>(rowCount));
}
