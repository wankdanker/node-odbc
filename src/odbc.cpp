/*
  Copyright (c) 2013, Dan VerWeire <dverweire@gmail.com>
  Copyright (c) 2010, Lee Smith<notwink@gmail.com>

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

#ifdef dynodbc
#include "dynodbc.h"
#endif

#ifdef _WIN32
#include "strptime.h"
#endif

using namespace v8;
using namespace node;

uv_mutex_t ODBC::g_odbcMutex;

Nan::Persistent<Function> ODBC::constructor;

void ODBC::Init(v8::Handle<Object> exports) {
  DEBUG_PRINTF("ODBC::Init\n");
  Nan::HandleScope scope;

  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(New);

  // Constructor Template
  constructor_template->SetClassName(Nan::New("ODBC").ToLocalChecked());

  // Reserve space for one Handle<Value>
  Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
  instance_template->SetInternalFieldCount(1);
  
  // Constants
#if (NODE_MODULE_VERSION < NODE_0_12_MODULE_VERSION)

#else

#endif
  PropertyAttribute constant_attributes = static_cast<PropertyAttribute>(ReadOnly | DontDelete);
  constructor_template->Set(Nan::New<String>("SQL_CLOSE").ToLocalChecked(), Nan::New<Number>(SQL_CLOSE), constant_attributes);
  constructor_template->Set(Nan::New<String>("SQL_DROP").ToLocalChecked(), Nan::New<Number>(SQL_DROP), constant_attributes);
  constructor_template->Set(Nan::New<String>("SQL_UNBIND").ToLocalChecked(), Nan::New<Number>(SQL_UNBIND), constant_attributes);
  constructor_template->Set(Nan::New<String>("SQL_RESET_PARAMS").ToLocalChecked(), Nan::New<Number>(SQL_RESET_PARAMS), constant_attributes);
  constructor_template->Set(Nan::New<String>("SQL_DESTROY").ToLocalChecked(), Nan::New<Number>(SQL_DESTROY), constant_attributes);
  constructor_template->Set(Nan::New<String>("FETCH_ARRAY").ToLocalChecked(), Nan::New<Number>(FETCH_ARRAY), constant_attributes);
  constructor_template->Set(Nan::New<String>("SQL_USER_NAME").ToLocalChecked(), Nan::New<Number>(SQL_USER_NAME), constant_attributes);
  NODE_ODBC_DEFINE_CONSTANT(constructor_template, FETCH_OBJECT);
  
  // Prototype Methods
  Nan::SetPrototypeMethod(constructor_template, "createConnection", CreateConnection);
  Nan::SetPrototypeMethod(constructor_template, "createConnectionSync", CreateConnectionSync);

  // Attach the Database Constructor to the target object
  constructor.Reset(constructor_template->GetFunction());
  exports->Set(Nan::New("ODBC").ToLocalChecked(),
               constructor_template->GetFunction());
  
  // Initialize the cross platform mutex provided by libuv
  uv_mutex_init(&ODBC::g_odbcMutex);
}

ODBC::~ODBC() {
  DEBUG_PRINTF("ODBC::~ODBC\n");
  this->Free();
}

void ODBC::Free() {
  DEBUG_PRINTF("ODBC::Free\n");
  if (m_hEnv) {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    if (m_hEnv) {
      SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
      m_hEnv = NULL;      
    }

    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
}

NAN_METHOD(ODBC::New) {
  DEBUG_PRINTF("ODBC::New\n");
  Nan::HandleScope scope;
  ODBC* dbo = new ODBC();
  
  dbo->Wrap(info.Holder());

  dbo->m_hEnv = NULL;
  
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  // Initialize the Environment handle
  int ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &dbo->m_hEnv);
  
  uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  if (!SQL_SUCCEEDED(ret)) {
    DEBUG_PRINTF("ODBC::New - ERROR ALLOCATING ENV HANDLE!!\n");
    
    Local<Value> objError = ODBC::GetSQLError(SQL_HANDLE_ENV, dbo->m_hEnv);
    
    return Nan::ThrowError(objError);
  }
  
  // Use ODBC 3.x behavior
  SQLSetEnvAttr(dbo->m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, SQL_IS_UINTEGER);
  
  info.GetReturnValue().Set(info.Holder());
}

//void ODBC::WatcherCallback(uv_async_t *w, int revents) {
//  DEBUG_PRINTF("ODBC::WatcherCallback\n");
//  //i don't know if we need to do anything here
//}

/*
 * CreateConnection
 */

NAN_METHOD(ODBC::CreateConnection) {
  DEBUG_PRINTF("ODBC::CreateConnection\n");
  Nan::HandleScope scope;

  Local<Function> cb = info[0].As<Function>();
  Nan::Callback *callback = new Nan::Callback(cb);
  //REQ_FUN_ARG(0, cb);

  ODBC* dbo = Nan::ObjectWrap::Unwrap<ODBC>(info.Holder());
  
  //initialize work request
  uv_work_t* work_req = (uv_work_t *) (calloc(1, sizeof(uv_work_t)));
  
  //initialize our data
  create_connection_work_data* data = 
    (create_connection_work_data *) (calloc(1, sizeof(create_connection_work_data)));

  data->cb = callback;
  data->dbo = dbo;

  work_req->data = data;
  
  uv_queue_work(uv_default_loop(), work_req, UV_CreateConnection, (uv_after_work_cb)UV_AfterCreateConnection);

  dbo->Ref();

  info.GetReturnValue().Set(Nan::Undefined());
}

void ODBC::UV_CreateConnection(uv_work_t* req) {
  DEBUG_PRINTF("ODBC::UV_CreateConnection\n");
  
  //get our work data
  create_connection_work_data* data = (create_connection_work_data *)(req->data);
  
  uv_mutex_lock(&ODBC::g_odbcMutex);

  //allocate a new connection handle
  data->result = SQLAllocHandle(SQL_HANDLE_DBC, data->dbo->m_hEnv, &data->hDBC);
  
  uv_mutex_unlock(&ODBC::g_odbcMutex);
}

void ODBC::UV_AfterCreateConnection(uv_work_t* req, int status) {
  DEBUG_PRINTF("ODBC::UV_AfterCreateConnection\n");
  Nan::HandleScope scope;

  create_connection_work_data* data = (create_connection_work_data *)(req->data);
  
  Nan::TryCatch try_catch;
  
  if (!SQL_SUCCEEDED(data->result)) {
    Local<Value> info[1];
    
    info[0] = ODBC::GetSQLError(SQL_HANDLE_ENV, data->dbo->m_hEnv);
    
    data->cb->Call(1, info);
  }
  else {
    Local<Value> info[2];
    info[0] = Nan::New<External>(data->dbo->m_hEnv);
    info[1] = Nan::New<External>(data->hDBC);
    
    Local<Object> js_result = Nan::NewInstance(Nan::New(ODBCConnection::constructor), 2, info).ToLocalChecked();

    info[0] = Nan::Null();
    info[1] = js_result;

    data->cb->Call(data->dbo->handle(), 2, info);
  }
  
  if (try_catch.HasCaught()) {
      Nan::FatalException(try_catch);
  }

  
  data->dbo->Unref();
  delete data->cb;

  free(data);
  free(req);
}

/*
 * CreateConnectionSync
 */

NAN_METHOD(ODBC::CreateConnectionSync) {
  DEBUG_PRINTF("ODBC::CreateConnectionSync\n");
  Nan::HandleScope scope;

  ODBC* dbo = Nan::ObjectWrap::Unwrap<ODBC>(info.Holder());
   
  HDBC hDBC;
  
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  //allocate a new connection handle
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_DBC, dbo->m_hEnv, &hDBC);
  
  if (!SQL_SUCCEEDED(ret)) {
    //TODO: do something!
  }
  
  uv_mutex_unlock(&ODBC::g_odbcMutex);

  Local<Value> params[2];
  params[0] = Nan::New<External>(dbo->m_hEnv);
  params[1] = Nan::New<External>(hDBC);

  Local<Object> js_result = Nan::NewInstance(Nan::New(ODBCConnection::constructor), 2, params).ToLocalChecked();

  info.GetReturnValue().Set(js_result);
}

/*
 * GetColumns
 */

Column* ODBC::GetColumns(SQLHSTMT hStmt, short* colCount) {
  SQLRETURN ret;
  SQLSMALLINT buflen;

  //always reset colCount for the current result set to 0;
  *colCount = 0; 

  //get the number of columns in the result set
  ret = SQLNumResultCols(hStmt, colCount);
  
  if (!SQL_SUCCEEDED(ret)) {
    return new Column[0];
  }
  
  Column *columns = new Column[*colCount];

  for (int i = 0; i < *colCount; i++) {
    //save the index number of this column
    columns[i].index = i + 1;
    //TODO:that's a lot of memory for each field name....
    columns[i].name = new unsigned char[MAX_FIELD_SIZE];
    
    //set the first byte of name to \0 instead of memsetting the entire buffer
    columns[i].name[0] = '\0';
    
    //get the column name
    ret = SQLColAttribute( hStmt,
                           columns[i].index,
#ifdef STRICT_COLUMN_NAMES
                           SQL_DESC_NAME,
#else
                           SQL_DESC_LABEL,
#endif
                           columns[i].name,
                           (SQLSMALLINT) MAX_FIELD_SIZE,
                           (SQLSMALLINT *) &buflen,
                           NULL);
    
    //store the len attribute
    columns[i].len = buflen;
    
    //get the column type and store it directly in column[i].type
    ret = SQLColAttribute( hStmt,
                           columns[i].index,
                           SQL_DESC_TYPE,
                           NULL,
                           0,
                           NULL,
                           &columns[i].type);
  }
  
  return columns;
}

/*
 * FreeColumns
 */

void ODBC::FreeColumns(Column* columns, short* colCount) {
  for(int i = 0; i < *colCount; i++) {
      delete [] columns[i].name;
  }

  delete [] columns;
  
  *colCount = 0;
}

/*
 * GetColumnValue
 */

Handle<Value> ODBC::GetColumnValue( SQLHSTMT hStmt, Column column, 
                                        uint16_t* buffer, int bufferLength) {
  Nan::EscapableHandleScope scope;
  SQLLEN len = 0;

  //reset the buffer
  buffer[0] = '\0';

  //TODO: SQLGetData can supposedly return multiple chunks, need to do this to 
  //retrieve large fields
  int ret; 
  
  switch ((int) column.type) {
    case SQL_INTEGER : 
    case SQL_SMALLINT :
    case SQL_TINYINT : {
        int32_t value = 0;
        
        ret = SQLGetData(
          hStmt, 
          column.index, 
          SQL_C_SLONG,
          &value, 
          sizeof(value), 
          &len);
        
        DEBUG_PRINTF("ODBC::GetColumnValue - Integer: index=%i name=%s type=%lli len=%lli ret=%i val=%li\n", 
                    column.index, column.name, column.type, len, ret, value);
        
        if (len == SQL_NULL_DATA) {
          return scope.Escape(Nan::Null());
        }
        else {
          return scope.Escape(Nan::New<Integer>(value));
        }
      }
      break;
    case SQL_NUMERIC :
    case SQL_DECIMAL :
    case SQL_BIGINT :
    case SQL_FLOAT :
    case SQL_REAL :
    case SQL_DOUBLE : {
        double value;
        
        ret = SQLGetData(
          hStmt, 
          column.index, 
          SQL_C_DOUBLE,
          &value, 
          sizeof(value), 
          &len);
        
         DEBUG_PRINTF("ODBC::GetColumnValue - Number: index=%i name=%s type=%lli len=%lli ret=%i val=%f\n", 
                    column.index, column.name, column.type, len, ret, value);
        
        if (len == SQL_NULL_DATA) {
          return scope.Escape(Nan::Null());
          //return Null();
        }
        else {
          return scope.Escape(Nan::New<Number>(value));
          //return Number::New(value);
        }
      }
      break;
    case SQL_DATETIME :
    case SQL_TIMESTAMP : {
      //I am not sure if this is locale-safe or cross database safe, but it 
      //works for me on MSSQL
#ifdef _WIN32
      struct tm timeInfo = {};

      ret = SQLGetData(
        hStmt, 
        column.index, 
        SQL_C_CHAR,
        (char *) buffer, 
        bufferLength, 
        &len);

      DEBUG_PRINTF("ODBC::GetColumnValue - W32 Timestamp: index=%i name=%s type=%lli len=%lli\n", 
                    column.index, column.name, column.type, len);

      if (len == SQL_NULL_DATA) {
        return scope.Escape(Nan::Null());
        //return Null();
      }
      else {
        if (strptime((char *) buffer, "%Y-%m-%d %H:%M:%S", &timeInfo)) {
          //a negative value means that mktime() should use timezone information
          //and system databases to attempt to determine whether DST is in effect
          //at the specified time.
          timeInfo.tm_isdst = -1;
          
          //return scope.Escape(Date::New(Isolate::GetCurrent(), (double(mktime(&timeInfo)) * 1000));
          return scope.Escape(Nan::New<Date>(double(mktime(&timeInfo)) * 1000).ToLocalChecked());
        }
        else {
          return scope.Escape(Nan::New((char *)buffer).ToLocalChecked());
        }
      }
#else
      struct tm timeInfo = { 
        tm_sec : 0
        , tm_min : 0
        , tm_hour : 0
        , tm_mday : 0
        , tm_mon : 0
        , tm_year : 0
        , tm_wday : 0
        , tm_yday : 0
        , tm_isdst : 0
        #ifndef _AIX //AIX does not have these 
        , tm_gmtoff : 0
        , tm_zone : 0
        #endif
      };

      SQL_TIMESTAMP_STRUCT odbcTime;
      
      ret = SQLGetData(
        hStmt, 
        column.index, 
        SQL_C_TYPE_TIMESTAMP,
        &odbcTime, 
        bufferLength, 
        &len);

      DEBUG_PRINTF("ODBC::GetColumnValue - Unix Timestamp: index=%i name=%s type=%i len=%i\n", 
                    column.index, column.name, column.type, len);

      if (len == SQL_NULL_DATA) {
        return scope.Escape(Nan::Null());
        //return Null();
      }
      else {
        timeInfo.tm_year = odbcTime.year - 1900;
        timeInfo.tm_mon = odbcTime.month - 1;
        timeInfo.tm_mday = odbcTime.day;
        timeInfo.tm_hour = odbcTime.hour;
        timeInfo.tm_min = odbcTime.minute;
        timeInfo.tm_sec = odbcTime.second;

        //a negative value means that mktime() should use timezone information 
        //and system databases to attempt to determine whether DST is in effect 
        //at the specified time.
        timeInfo.tm_isdst = -1;
#ifdef TIMEGM
        return scope.Escape(Nan::New<Date>((double(timegm(&timeInfo)) * 1000)
                          + (odbcTime.fraction / 1000000)).ToLocalChecked());
#else
#ifdef _AIX
    #define timelocal mktime
#endif
        return scope.Escape(Nan::New<Date>((double(timelocal(&timeInfo)) * 1000)
                          + (odbcTime.fraction / 1000000)).ToLocalChecked());
#endif
        //return Date::New((double(timegm(&timeInfo)) * 1000) 
        //                  + (odbcTime.fraction / 1000000));
      }
#endif
    } break;
    case SQL_BIT :
      //again, i'm not sure if this is cross database safe, but it works for 
      //MSSQL
      ret = SQLGetData(
        hStmt, 
        column.index, 
        SQL_C_CHAR,
        (char *) buffer, 
        bufferLength, 
        &len);

      DEBUG_PRINTF("ODBC::GetColumnValue - Bit: index=%i name=%s type=%lli len=%lli\n", 
                    column.index, column.name, column.type, len);

      if (len == SQL_NULL_DATA) {
        return scope.Escape(Nan::Null());
      }
      else {
        return scope.Escape(Nan::New((*buffer == '0') ? false : true));
      }
    default :
      Local<String> str;
      int count = 0;
      
      do {
        ret = SQLGetData(
          hStmt,
          column.index,
          SQL_C_TCHAR,
          (char *) buffer,
          bufferLength,
          &len);

        DEBUG_PRINTF("ODBC::GetColumnValue - String: index=%i name=%s type=%lli len=%lli value=%s ret=%i bufferLength=%i\n", 
                      column.index, column.name, column.type, len,(char *) buffer, ret, bufferLength);

        if (len == SQL_NULL_DATA && str.IsEmpty()) {
          return scope.Escape(Nan::Null());
          //return Null();
        }
        
        if (SQL_NO_DATA == ret) {
          //we have captured all of the data
          //double check that we have some data else return null
          if (str.IsEmpty()){
            return scope.Escape(Nan::Null());
          }

          break;
        }
        else if (SQL_SUCCEEDED(ret)) {
          //we have not captured all of the data yet
          
          if (count == 0) {
            //no concatenation required, this is our first pass
#ifdef UNICODE
            str = Nan::New((uint16_t*) buffer).ToLocalChecked();
#else
            str = Nan::New((char *) buffer).ToLocalChecked();
#endif
          }
          else {
            //we need to concatenate
#ifdef UNICODE
            str = String::Concat(str, Nan::New((uint16_t*) buffer).ToLocalChecked());
#else
            str = String::Concat(str, Nan::New((char *) buffer).ToLocalChecked());
#endif
          }
          
          //if len is zero let's break out of the loop now and not attempt to
          //call SQLGetData again. The specific reason for this is because
          //some ODBC drivers may not correctly report SQL_NO_DATA the next
          //time around causing an infinite loop here
          if (len == 0) {
            break;
          }
          
          //hack: if we have a success with info and the buffer wasn't filled
          //then there is probably an issue with the driver not correctly reporting
          //SQL_NO_DATA the next time around. This has been seen with MSSQL
          //sql_variant fields on FreeTDS.
          if (SQL_SUCCESS_WITH_INFO == ret && len < bufferLength) {
            break;
          }

          count += 1;
        }
        else {
          //an error has occured
          //possible values for ret are SQL_ERROR (-1) and SQL_INVALID_HANDLE (-2)

          //If we have an invalid handle, then stuff is way bad and we should abort
          //immediately. Memory errors are bound to follow as we must be in an
          //inconsisant state.
          assert(ret != SQL_INVALID_HANDLE);

          //Not sure if throwing here will work out well for us but we can try
          //since we should have a valid handle and the error is something we 
          //can look into

          Local<Value> objError = ODBC::GetSQLError(
            SQL_HANDLE_STMT,
            hStmt,
            (char *) "[node-odbc] Error in ODBC::GetColumnValue"
          );

          Nan::ThrowError(objError);
          return scope.Escape(Nan::Undefined());
          break;
        }
      } while (true);
      
      return scope.Escape(str);
  }
}

/*
 * GetRecordTuple
 */

Local<Value> ODBC::GetRecordTuple ( SQLHSTMT hStmt, Column* columns, 
                                         short* colCount, uint16_t* buffer,
                                         int bufferLength) {
  Nan::EscapableHandleScope scope;
  
  Local<Object> tuple = Nan::New<Object>();
        
  for(int i = 0; i < *colCount; i++) {
#ifdef UNICODE
    tuple->Set( Nan::New((uint16_t *) columns[i].name).ToLocalChecked(),
                GetColumnValue( hStmt, columns[i], buffer, bufferLength));
#else
    tuple->Set( Nan::New((const char *) columns[i].name).ToLocalChecked(),
                GetColumnValue( hStmt, columns[i], buffer, bufferLength));
#endif
  }
  
  return scope.Escape(tuple);
}

/*
 * GetRecordArray
 */

Local<Value> ODBC::GetRecordArray ( SQLHSTMT hStmt, Column* columns, 
                                         short* colCount, uint16_t* buffer,
                                         int bufferLength) {
  Nan::EscapableHandleScope scope;
  
  Local<Array> array = Nan::New<Array>();
        
  for(int i = 0; i < *colCount; i++) {
    array->Set( Nan::New(i),
                GetColumnValue( hStmt, columns[i], buffer, bufferLength));
  }
  
  return scope.Escape(array);
}

/*
 * GetParametersFromArray
 */

Parameter* ODBC::GetParametersFromArray (Local<Array> values, int *paramCount) {
  DEBUG_PRINTF("ODBC::GetParametersFromArray\n");
  *paramCount = values->Length();
  
  Parameter* params = NULL;
  
  if (*paramCount > 0) {
    params = (Parameter *) malloc(*paramCount * sizeof(Parameter));
  }
  
  for (int i = 0; i < *paramCount; i++) {
    Local<Value> value = values->Get(i);
    
    params[i].ColumnSize       = 0;
    params[i].StrLen_or_IndPtr = SQL_NULL_DATA;
    params[i].BufferLength     = 0;
    params[i].DecimalDigits    = 0;

    DEBUG_PRINTF("ODBC::GetParametersFromArray - param[%i].length = %lli\n",
                 i, params[i].StrLen_or_IndPtr);

    if (value->IsString()) {
      Local<String> string = value->ToString();
      
      params[i].ValueType         = SQL_C_TCHAR;
      params[i].ColumnSize        = 0; //SQL_SS_LENGTH_UNLIMITED 
#ifdef UNICODE
      params[i].ParameterType     = SQL_WVARCHAR;
      params[i].BufferLength      = (string->Length() * sizeof(uint16_t)) + sizeof(uint16_t);
#else
      params[i].ParameterType     = SQL_VARCHAR;
      params[i].BufferLength      = string->Utf8Length() + 1;
#endif
      params[i].ParameterValuePtr = malloc(params[i].BufferLength);
      params[i].StrLen_or_IndPtr  = SQL_NTS;//params[i].BufferLength;

#ifdef UNICODE
      string->Write((uint16_t *) params[i].ParameterValuePtr);
#else
      string->WriteUtf8((char *) params[i].ParameterValuePtr);
#endif

      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsString(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli value=%s\n",
                    i, params[i].ValueType, params[i].ParameterType,
                    params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr, 
                    (char*) params[i].ParameterValuePtr);
    }
    else if (value->IsNull()) {
      params[i].ValueType = SQL_C_DEFAULT;
      params[i].ParameterType   = SQL_VARCHAR;
      params[i].StrLen_or_IndPtr = SQL_NULL_DATA;

      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsNull(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli\n",
                   i, params[i].ValueType, params[i].ParameterType,
                   params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr);
    }
    else if (value->IsInt32()) {
      int64_t  *number = new int64_t(value->IntegerValue());
      params[i].ValueType = SQL_C_SBIGINT;
      params[i].ParameterType   = SQL_BIGINT;
      params[i].ParameterValuePtr = number;
      params[i].StrLen_or_IndPtr = 0;
      
      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsInt32(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli value=%lld\n",
                    i, params[i].ValueType, params[i].ParameterType,
                    params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr,
                    *number);
    }
    else if (value->IsNumber()) {
      double *number   = new double(value->NumberValue());
      
      params[i].ValueType         = SQL_C_DOUBLE;
      params[i].ParameterType     = SQL_DECIMAL;
      params[i].ParameterValuePtr = number;
      params[i].BufferLength      = sizeof(double);
      params[i].StrLen_or_IndPtr  = params[i].BufferLength;
      params[i].DecimalDigits     = 7;
      params[i].ColumnSize        = sizeof(double);

      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsNumber(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli value=%f\n",
                    i, params[i].ValueType, params[i].ParameterType,
                    params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr,
		                *number);
    }
    else if (value->IsBoolean()) {
      bool *boolean = new bool(value->BooleanValue());
      params[i].ValueType         = SQL_C_BIT;
      params[i].ParameterType     = SQL_BIT;
      params[i].ParameterValuePtr = boolean;
      params[i].StrLen_or_IndPtr  = 0;
      
      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsBoolean(): params[%i] c_type=%i type=%i buffer_length=%lli size=%lli length=%lli\n",
                   i, params[i].ValueType, params[i].ParameterType,
                   params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr);
    }
  } 
  
  return params;
}

/*
 * CallbackSQLError
 */

Handle<Value> ODBC::CallbackSQLError (SQLSMALLINT handleType,
                                      SQLHANDLE handle, 
                                      Nan::Callback* cb) {
  Nan::EscapableHandleScope scope;
  
  return scope.Escape(CallbackSQLError(
    handleType,
    handle,
    (char *) "[node-odbc] SQL_ERROR",
    cb));
}

Local<Value> ODBC::CallbackSQLError (SQLSMALLINT handleType,
                                      SQLHANDLE handle,
                                      char* message,
                                      Nan::Callback* cb) {
  Nan::EscapableHandleScope scope;
  
  Local<Object> objError = ODBC::GetSQLError(
    handleType, 
    handle, 
    message
  );
  
  Local<Value> info[1];
  info[0] = objError;
  cb->Call(1, info);
  
  return scope.Escape(Nan::Undefined());
}

/*
 * GetSQLError
 */

Local<Object> ODBC::GetSQLError (SQLSMALLINT handleType, SQLHANDLE handle) {
  Nan::EscapableHandleScope scope;
  
  return scope.Escape(GetSQLError(
    handleType,
    handle,
    (char *) "[node-odbc] SQL_ERROR"));
}

Local<Object> ODBC::GetSQLError (SQLSMALLINT handleType, SQLHANDLE handle, char* message) {
  Nan::EscapableHandleScope scope;
  
  DEBUG_PRINTF("ODBC::GetSQLError : handleType=%i, handle=%p\n", handleType, handle);
  
  Local<Object> objError = Nan::New<Object>();

  int32_t i = 0;
  SQLINTEGER native;
  
  SQLSMALLINT len;
  SQLINTEGER statusRecCount;
  SQLRETURN ret;
  char errorSQLState[14];
  char errorMessage[ERROR_MESSAGE_BUFFER_BYTES];

  ret = SQLGetDiagField(
    handleType,
    handle,
    0,
    SQL_DIAG_NUMBER,
    &statusRecCount,
    SQL_IS_INTEGER,
    &len);

  // Windows seems to define SQLINTEGER as long int, unixodbc as just int... %i should cover both
  DEBUG_PRINTF("ODBC::GetSQLError : called SQLGetDiagField; ret=%i, statusRecCount=%i\n", ret, statusRecCount);

  Local<Array> errors = Nan::New<Array>();
  objError->Set(Nan::New("errors").ToLocalChecked(), errors);
  
  for (i = 0; i < statusRecCount; i++){
    DEBUG_PRINTF("ODBC::GetSQLError : calling SQLGetDiagRec; i=%i, statusRecCount=%i\n", i, statusRecCount);
    
    ret = SQLGetDiagRec(
      handleType, 
      handle,
      (SQLSMALLINT)(i + 1), 
      (SQLTCHAR *) errorSQLState,
      &native,
      (SQLTCHAR *) errorMessage,
      ERROR_MESSAGE_BUFFER_CHARS,
      &len);
    
    DEBUG_PRINTF("ODBC::GetSQLError : after SQLGetDiagRec; i=%i\n", i);

    if (SQL_SUCCEEDED(ret)) {
      DEBUG_PRINTF("ODBC::GetSQLError : errorMessage=%s, errorSQLState=%s\n", errorMessage, errorSQLState);
      
      if (i == 0) {
        // First error is assumed the primary error
        objError->Set(Nan::New("error").ToLocalChecked(), Nan::New(message).ToLocalChecked());
#ifdef UNICODE
        Nan::SetPrototype(objError, Exception::Error(Nan::New((uint16_t *) errorMessage).ToLocalChecked()));
        objError->Set(Nan::New("message").ToLocalChecked(), Nan::New((uint16_t *)errorMessage).ToLocalChecked());
        objError->Set(Nan::New("state").ToLocalChecked(), Nan::New((uint16_t *)errorSQLState).ToLocalChecked());
#else
        Nan::SetPrototype(objError, Exception::Error(Nan::New(errorMessage).ToLocalChecked()));
        objError->Set(Nan::New("message").ToLocalChecked(), Nan::New(errorMessage).ToLocalChecked());
        objError->Set(Nan::New("state").ToLocalChecked(), Nan::New(errorSQLState).ToLocalChecked());
#endif
      }

      Local<Object> subError = Nan::New<Object>();

#ifdef UNICODE
      subError->Set(Nan::New("message").ToLocalChecked(), Nan::New((uint16_t *)errorMessage).ToLocalChecked());
      subError->Set(Nan::New("state").ToLocalChecked(), Nan::New((uint16_t *)errorSQLState).ToLocalChecked());
#else
      subError->Set(Nan::New("message").ToLocalChecked(), Nan::New(errorMessage).ToLocalChecked());
      subError->Set(Nan::New("state").ToLocalChecked(), Nan::New(errorSQLState).ToLocalChecked());
#endif
      errors->Set(Nan::New(i), subError);

    } else if (ret == SQL_NO_DATA) {
      break;
    }
  }

  if (statusRecCount == 0) {
    //Create a default error object if there were no diag records
    objError->Set(Nan::New("error").ToLocalChecked(), Nan::New(message).ToLocalChecked());
    Nan::SetPrototype(objError, Exception::Error(Nan::New(message).ToLocalChecked()));
    objError->Set(Nan::New("message").ToLocalChecked(), Nan::New(
      (const char *) "[node-odbc] An error occurred but no diagnostic information was available.").ToLocalChecked());
  }

  return scope.Escape(objError);
}

/*
 * GetAllRecordsSync
 */

Local<Array> ODBC::GetAllRecordsSync (HENV hENV, 
                                     HDBC hDBC, 
                                     HSTMT hSTMT,
                                     uint16_t* buffer,
                                     int bufferLength) {
  DEBUG_PRINTF("ODBC::GetAllRecordsSync\n");
  
  Nan::EscapableHandleScope scope;
  
  Local<Object> objError = Nan::New<Object>();
  
  int count = 0;
  int errorCount = 0;
  short colCount = 0;
  
  Column* columns = GetColumns(hSTMT, &colCount);
  
  Local<Array> rows = Nan::New<Array>();
  
  //loop through all records
  while (true) {
    SQLRETURN ret = SQLFetch(hSTMT);
    
    //check to see if there was an error
    if (ret == SQL_ERROR)  {
      //TODO: what do we do when we actually get an error here...
      //should we throw??
      
      errorCount++;
      
      objError = ODBC::GetSQLError(
        SQL_HANDLE_STMT, 
        hSTMT,
        (char *) "[node-odbc] Error in ODBC::GetAllRecordsSync"
      );
      
      break;
    }
    
    //check to see if we are at the end of the recordset
    if (ret == SQL_NO_DATA) {
      ODBC::FreeColumns(columns, &colCount);
      
      break;
    }

    rows->Set(
      Nan::New(count), 
      ODBC::GetRecordTuple(
        hSTMT,
        columns,
        &colCount,
        buffer,
        bufferLength)
    );

    count++;
  }
  //TODO: what do we do about errors!?!
  //we throw them
  return scope.Escape(rows);
}

#ifdef dynodbc
NAN_METHOD(ODBC::LoadODBCLibrary) {
  Nan::HandleScope scope;
  
  REQ_STR_ARG(0, js_library);
  
  bool result = DynLoadODBC(*js_library);
  
  info.GetReturnValue().Set((result) ? Nan::True() : Nan::False());
}
#endif

extern "C" void init(v8::Handle<Object> exports) {
#ifdef dynodbc
  exports->Set(Nan::New("loadODBCLibrary").ToLocalChecked(),
        Nan::New<FunctionTemplate>(ODBC::LoadODBCLibrary)->GetFunction());
#endif
  
  ODBC::Init(exports);
  ODBCResult::Init(exports);
  ODBCConnection::Init(exports);
  ODBCStatement::Init(exports);
}

NODE_MODULE(odbc_bindings, init)
