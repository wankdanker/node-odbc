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

using namespace v8;
using namespace node;

Nan::Persistent<Function> ODBCConnection::constructor;
Nan::Persistent<String> ODBCConnection::OPTION_SQL;
Nan::Persistent<String> ODBCConnection::OPTION_PARAMS;
Nan::Persistent<String> ODBCConnection::OPTION_NORESULTS;

void ODBCConnection::Init(Local<Object> exports) {
  DEBUG_PRINTF("ODBCConnection::Init\n");
  Nan::HandleScope scope;

  OPTION_SQL.Reset(Nan::New<String>("sql").ToLocalChecked());
  OPTION_PARAMS.Reset(Nan::New<String>("params").ToLocalChecked());
  OPTION_NORESULTS.Reset(Nan::New<String>("noResults").ToLocalChecked());

  Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);

  // Constructor Template
  t->SetClassName(Nan::New("ODBCConnection").ToLocalChecked());

  // Reserve space for one Handle<Value>
  Local<ObjectTemplate> i = t->InstanceTemplate();
  i->SetInternalFieldCount(1);
  
  // Properties
  //Nan::SetAccessor(i, Nan::New("mode").ToLocalChecked(), ModeGetter, ModeSetter);
  Nan::SetAccessor(i, Nan::New("connected").ToLocalChecked(), ConnectedGetter);
  Nan::SetAccessor(i, Nan::New("connectTimeout").ToLocalChecked(), ConnectTimeoutGetter, ConnectTimeoutSetter);
  Nan::SetAccessor(i, Nan::New("loginTimeout").ToLocalChecked(), LoginTimeoutGetter, LoginTimeoutSetter);
  
  // Prototype Methods
  Nan::SetPrototypeMethod(t, "open", Open);
  Nan::SetPrototypeMethod(t, "openSync", OpenSync);
  Nan::SetPrototypeMethod(t, "close", Close);
  Nan::SetPrototypeMethod(t, "closeSync", CloseSync);
  Nan::SetPrototypeMethod(t, "createStatement", CreateStatement);
  Nan::SetPrototypeMethod(t, "createStatementSync", CreateStatementSync);
  Nan::SetPrototypeMethod(t, "query", Query);
  Nan::SetPrototypeMethod(t, "querySync", QuerySync);
  
  Nan::SetPrototypeMethod(t, "beginTransaction", BeginTransaction);
  Nan::SetPrototypeMethod(t, "beginTransactionSync", BeginTransactionSync);
  Nan::SetPrototypeMethod(t, "endTransaction", EndTransaction);
  Nan::SetPrototypeMethod(t, "endTransactionSync", EndTransactionSync);

  Nan::SetPrototypeMethod(t, "getInfoSync", GetInfoSync);

  Nan::SetPrototypeMethod(t, "columns", Columns);
  Nan::SetPrototypeMethod(t, "tables", Tables);
  
  // Attach the Database Constructor to the target object
  constructor.Reset(Nan::GetFunction(t).ToLocalChecked());

  Nan::Set(exports,
    Nan::New("ODBCConnection").ToLocalChecked(),
    Nan::GetFunction(t).ToLocalChecked());
}

ODBCConnection::~ODBCConnection() {
  DEBUG_PRINTF("ODBCConnection::~ODBCConnection\n");
  this->Free();
}

void ODBCConnection::Free() {
  DEBUG_PRINTF("ODBCConnection::Free\n");
  if (m_hDBC) {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    if (m_hDBC) {
      SQLDisconnect(m_hDBC);
      SQLFreeHandle(SQL_HANDLE_DBC, m_hDBC);
      m_hDBC = NULL;
    }
    
    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
}

/*
 * New
 */

NAN_METHOD(ODBCConnection::New) {
  DEBUG_PRINTF("ODBCConnection::New\n");
  Nan::HandleScope scope;
  
  REQ_EXT_ARG(0, js_henv);
  REQ_EXT_ARG(1, js_hdbc);
  
  HENV hENV = static_cast<HENV>(js_henv->Value());
  HDBC hDBC = static_cast<HDBC>(js_hdbc->Value());
  
  ODBCConnection* conn = new ODBCConnection(hENV, hDBC);
  
  conn->Wrap(info.Holder());
  
  //set default connectTimeout to 0 seconds
  conn->connectTimeout = 0;
  //set default loginTimeout to 5 seconds
  conn->loginTimeout = 5;

  info.GetReturnValue().Set(info.Holder());
}

NAN_GETTER(ODBCConnection::ConnectedGetter) {
  Nan::HandleScope scope;

  ODBCConnection *obj = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());

  info.GetReturnValue().Set(obj->connected ? Nan::True() : Nan::False());
}

NAN_GETTER(ODBCConnection::ConnectTimeoutGetter) {
  Nan::HandleScope scope;

  ODBCConnection *obj = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());

  info.GetReturnValue().Set(Nan::New<Number>(obj->connectTimeout));
}

NAN_SETTER(ODBCConnection::ConnectTimeoutSetter) {
  Nan::HandleScope scope;

  ODBCConnection *obj = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
  
  if (value->IsNumber()) {
    obj->connectTimeout = Nan::To<uint32_t>(value).FromJust();
  }
}

NAN_GETTER(ODBCConnection::LoginTimeoutGetter) {
  Nan::HandleScope scope;

  ODBCConnection *obj = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());

  info.GetReturnValue().Set(Nan::New<Number>(obj->loginTimeout));
}

NAN_SETTER(ODBCConnection::LoginTimeoutSetter) {
  Nan::HandleScope scope;

  ODBCConnection *obj = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
  
  if (value->IsNumber()) {
    obj->loginTimeout = Nan::To<uint32_t>(value).FromJust();
  }
}

/*
 * Open
 * 
 */

//Handle<Value> ODBCConnection::Open(const Arguments& info) {
NAN_METHOD(ODBCConnection::Open) {
  DEBUG_PRINTF("ODBCConnection::Open\n");
  Nan::HandleScope scope;

  REQ_STRO_ARG(0, connection);
  REQ_FUN_ARG(1, cb);

  //get reference to the connection object
  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
  
  //create a uv work request
  uv_work_t* work_req = (uv_work_t *) (calloc(1, sizeof(uv_work_t)));
 
  //allocate our worker data
  open_connection_work_data* data = (open_connection_work_data *) 
    calloc(1, sizeof(open_connection_work_data));

  //copy the connection string to the work data  
#ifdef UNICODE
  data->connectionLength = connection->Length() + 1;
  data->connection = (uint16_t *) malloc(sizeof(uint16_t) * data->connectionLength);
  STRING_WRITE(connection, info.GetIsolate(), (uint16_t*) data->connection);
#else
  data->connectionLength = connection->Utf8Length() + 1;
  data->connection = (char *) malloc(sizeof(char) * data->connectionLength);
  STRING_WRITEUTF8(connection, info.GetIsolate(), (char*) data->connection);
#endif
  
  data->cb = new Nan::Callback(cb);
  data->conn = conn;
  
  work_req->data = data;
  
  //queue the work
  uv_queue_work(uv_default_loop(), 
    work_req, 
    UV_Open, 
    (uv_after_work_cb)UV_AfterOpen);

  conn->Ref();

  info.GetReturnValue().Set(info.Holder());
}

void ODBCConnection::UV_Open(uv_work_t* req) {
  DEBUG_PRINTF("ODBCConnection::UV_Open\n");
  open_connection_work_data* data = (open_connection_work_data *)(req->data);
  
  ODBCConnection* self = data->conn->self();

  DEBUG_PRINTF("ODBCConnection::UV_Open : connectTimeout=%i, loginTimeout = %i\n", *&(self->connectTimeout), *&(self->loginTimeout));
  
  uv_mutex_lock(&ODBC::g_odbcMutex); 
  
  if (self->connectTimeout > 0) {
    //NOTE: SQLSetConnectAttr requires the thread to be locked
    SQLSetConnectAttr(
      self->m_hDBC,                              //ConnectionHandle
      SQL_ATTR_CONNECTION_TIMEOUT,               //Attribute
      (SQLPOINTER) size_t(self->connectTimeout), //ValuePtr
      SQL_IS_UINTEGER);                          //StringLength
  }
  
  if (self->loginTimeout > 0) {
    //NOTE: SQLSetConnectAttr requires the thread to be locked
    SQLSetConnectAttr(
      self->m_hDBC,                            //ConnectionHandle
      SQL_ATTR_LOGIN_TIMEOUT,                  //Attribute
      (SQLPOINTER) size_t(self->loginTimeout), //ValuePtr
      SQL_IS_UINTEGER);                        //StringLength
  }
  
  //Attempt to connect
  //NOTE: SQLDriverConnect requires the thread to be locked
  int ret = SQLDriverConnect(
    self->m_hDBC,                   //ConnectionHandle
    NULL,                           //WindowHandle
    (SQLTCHAR*) data->connection,   //InConnectionString
    data->connectionLength,         //StringLength1
    NULL,                           //OutConnectionString
    0,                              //BufferLength - in characters
    NULL,                           //StringLength2Ptr
    SQL_DRIVER_NOPROMPT);           //DriverCompletion
  
  if (SQL_SUCCEEDED(ret)) {
    HSTMT hStmt;
    
    //allocate a temporary statment
    ret = SQLAllocHandle(SQL_HANDLE_STMT, self->m_hDBC, &hStmt);
    
    //try to determine if the driver can handle
    //multiple recordsets
    ret = SQLGetFunctions(
      self->m_hDBC,
      SQL_API_SQLMORERESULTS, 
      &(self->canHaveMoreResults));

    if (!SQL_SUCCEEDED(ret)) {
      self->canHaveMoreResults = 0;
    }
    
    //free the handle
    ret = SQLFreeHandle( SQL_HANDLE_STMT, hStmt);
  }

  uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  data->result = ret;
}

void ODBCConnection::UV_AfterOpen(uv_work_t* req, int status) {
  DEBUG_PRINTF("ODBCConnection::UV_AfterOpen\n");
  Nan::HandleScope scope;
  
  open_connection_work_data* data = (open_connection_work_data *)(req->data);
  
  Local<Value> argv[1];
  
  bool err = false;

  if (data->result) {
    err = true;

    Local<Object> objError = ODBC::GetSQLError(SQL_HANDLE_DBC, data->conn->self()->m_hDBC);
    
    argv[0] = objError;
  }

  if (!err) {
   data->conn->self()->connected = true;
  }

  Nan::TryCatch try_catch;

  data->conn->Unref();
  Nan::Call(*data->cb, data->conn->handle(), err ? 1 : 0, argv);

  if (try_catch.HasCaught()) {
    Nan::FatalException(try_catch);
  }

  delete data->cb;
  
  free(data->connection);
  free(data);
  free(req);
}

/*
 * OpenSync
 */

NAN_METHOD(ODBCConnection::OpenSync) {
  DEBUG_PRINTF("ODBCConnection::OpenSync\n");
  Nan::HandleScope scope;

  REQ_STRO_ARG(0, connection);

  //get reference to the connection object
  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
 
  DEBUG_PRINTF("ODBCConnection::OpenSync : connectTimeout=%i, loginTimeout = %i\n", *&(conn->connectTimeout), *&(conn->loginTimeout));

  Local<Value> objError;
  SQLRETURN ret;
  bool err = false;
  
#ifdef UNICODE
  int connectionLength = connection->Length() + 1;
  uint16_t* connectionString = (uint16_t *) malloc(connectionLength * sizeof(uint16_t));
  STRING_WRITE(connection, info.GetIsolate(), connectionString);
#else
  int connectionLength = connection->Utf8Length() + 1;
  char* connectionString = (char *) malloc(connectionLength);
  STRING_WRITEUTF8(connection, info.GetIsolate(), connectionString);
#endif
  
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  if (conn->connectTimeout > 0) {
    //NOTE: SQLSetConnectAttr requires the thread to be locked
    SQLSetConnectAttr(
      conn->m_hDBC,                              //ConnectionHandle
      SQL_ATTR_CONNECTION_TIMEOUT,               //Attribute
      (SQLPOINTER) size_t(conn->connectTimeout), //ValuePtr
      SQL_IS_UINTEGER);                          //StringLength
  }

  if (conn->loginTimeout > 0) {
    //NOTE: SQLSetConnectAttr requires the thread to be locked
    SQLSetConnectAttr(
      conn->m_hDBC,                            //ConnectionHandle
      SQL_ATTR_LOGIN_TIMEOUT,                  //Attribute
      (SQLPOINTER) size_t(conn->loginTimeout), //ValuePtr
      SQL_IS_UINTEGER);                        //StringLength
  }
  
  //Attempt to connect
  //NOTE: SQLDriverConnect requires the thread to be locked
  ret = SQLDriverConnect(
    conn->m_hDBC,                   //ConnectionHandle
    NULL,                           //WindowHandle
    (SQLTCHAR*) connectionString,   //InConnectionString
    connectionLength,               //StringLength1
    NULL,                           //OutConnectionString
    0,                              //BufferLength - in characters
    NULL,                           //StringLength2Ptr
    SQL_DRIVER_NOPROMPT);           //DriverCompletion

  if (!SQL_SUCCEEDED(ret)) {
    err = true;
    
    objError = ODBC::GetSQLError(SQL_HANDLE_DBC, conn->self()->m_hDBC);
  }
  else {
    HSTMT hStmt;
    
    //allocate a temporary statment
    ret = SQLAllocHandle(SQL_HANDLE_STMT, conn->m_hDBC, &hStmt);
    
    //try to determine if the driver can handle
    //multiple recordsets
    ret = SQLGetFunctions(
      conn->m_hDBC,
      SQL_API_SQLMORERESULTS, 
      &(conn->canHaveMoreResults));

    if (!SQL_SUCCEEDED(ret)) {
      conn->canHaveMoreResults = 0;
    }
  
    //free the handle
    ret = SQLFreeHandle( SQL_HANDLE_STMT, hStmt);
    
    conn->self()->connected = true;
  }

  uv_mutex_unlock(&ODBC::g_odbcMutex);

  free(connectionString);
  
  if (err) {
    return Nan::ThrowError(objError);
  }
  else {
    info.GetReturnValue().Set(Nan::True());
  }
}

/*
 * Close
 * 
 */

NAN_METHOD(ODBCConnection::Close) {
  DEBUG_PRINTF("ODBCConnection::Close\n");
  Nan::HandleScope scope;

  REQ_FUN_ARG(0, cb);

  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
  
  uv_work_t* work_req = (uv_work_t *) (calloc(1, sizeof(uv_work_t)));
  
  close_connection_work_data* data = (close_connection_work_data *) 
    (calloc(1, sizeof(close_connection_work_data)));

  data->cb = new Nan::Callback(cb);
  data->conn = conn;

  work_req->data = data;
  
  uv_queue_work(
    uv_default_loop(),
    work_req,
    UV_Close,
    (uv_after_work_cb)UV_AfterClose);

  conn->Ref();

  info.GetReturnValue().Set(Nan::Undefined());
}

void ODBCConnection::UV_Close(uv_work_t* req) {
  DEBUG_PRINTF("ODBCConnection::UV_Close\n");
  close_connection_work_data* data = (close_connection_work_data *)(req->data);
  ODBCConnection* conn = data->conn;
  
  //TODO: check to see if there are any open statements
  //on this connection
  
  conn->Free();
  
  data->result = 0;
}

void ODBCConnection::UV_AfterClose(uv_work_t* req, int status) {
  DEBUG_PRINTF("ODBCConnection::UV_AfterClose\n");
  Nan::HandleScope scope;

  close_connection_work_data* data = (close_connection_work_data *)(req->data);

  ODBCConnection* conn = data->conn;
  
  Local<Value> argv[1];
  bool err = false;
  
  if (data->result) {
    err = true;
    argv[0] = Exception::Error(Nan::New("Error closing database").ToLocalChecked());
  }
  else {
    conn->connected = false;
  }

  Nan::TryCatch try_catch;

  data->conn->Unref();
  Nan::Call(*data->cb, err ? 1 : 0, argv);

  if (try_catch.HasCaught()) {
    Nan::FatalException(try_catch);
  }

  delete data->cb;

  free(data);
  free(req);
}

/*
 * CloseSync
 */

NAN_METHOD(ODBCConnection::CloseSync) {
  DEBUG_PRINTF("ODBCConnection::CloseSync\n");
  Nan::HandleScope scope;

  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
  
  //TODO: check to see if there are any open statements
  //on this connection
  
  conn->Free();
  
  conn->connected = false;

  info.GetReturnValue().Set(Nan::True());
}

/*
 * CreateStatementSync
 * 
 */

NAN_METHOD(ODBCConnection::CreateStatementSync) {
  DEBUG_PRINTF("ODBCConnection::CreateStatementSync\n");
  Nan::HandleScope scope;

  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
   
  HSTMT hSTMT;

  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  SQLAllocHandle(
    SQL_HANDLE_STMT, 
    conn->m_hDBC, 
    &hSTMT);
  
  uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  Local<Value> params[3];
  params[0] = Nan::New<External>(conn->m_hENV);
  params[1] = Nan::New<External>(conn->m_hDBC);
  params[2] = Nan::New<External>(hSTMT);
  
  Local<Object> js_result(Nan::NewInstance(Nan::New(ODBCStatement::constructor), 3, params).ToLocalChecked());
  
  info.GetReturnValue().Set(js_result);
}

/*
 * CreateStatement
 * 
 */

NAN_METHOD(ODBCConnection::CreateStatement) {
  DEBUG_PRINTF("ODBCConnection::CreateStatement\n");
  Nan::HandleScope scope;

  REQ_FUN_ARG(0, cb);

  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
    
  //initialize work request
  uv_work_t* work_req = (uv_work_t *) (calloc(1, sizeof(uv_work_t)));
  
  //initialize our data
  create_statement_work_data* data = 
    (create_statement_work_data *) (calloc(1, sizeof(create_statement_work_data)));

  data->cb = new Nan::Callback(cb);
  data->conn = conn;

  work_req->data = data;
  
  uv_queue_work(
    uv_default_loop(), 
    work_req, 
    UV_CreateStatement, 
    (uv_after_work_cb)UV_AfterCreateStatement);

  conn->Ref();

  info.GetReturnValue().Set(Nan::Undefined());
}

void ODBCConnection::UV_CreateStatement(uv_work_t* req) {
  DEBUG_PRINTF("ODBCConnection::UV_CreateStatement\n");
  
  //get our work data
  create_statement_work_data* data = (create_statement_work_data *)(req->data);

  DEBUG_PRINTF("ODBCConnection::UV_CreateStatement\n");
  //DEBUG_PRINTF("ODBCConnection::UV_CreateStatement m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
  //  data->conn->m_hENV,
  //  data->conn->m_hDBC,
  //  data->hSTMT
  //);
  
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  //allocate a new statment handle
  SQLAllocHandle( SQL_HANDLE_STMT, 
                  data->conn->m_hDBC, 
                  &data->hSTMT);

  uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  DEBUG_PRINTF("ODBCConnection::UV_CreateStatement\n");
  //DEBUG_PRINTF("ODBCConnection::UV_CreateStatement m_hDBC=%X m_hDBC=%X m_hSTMT=%X\n",
  //  data->conn->m_hENV,
  //  data->conn->m_hDBC,
  //  data->hSTMT
  //);
}

void ODBCConnection::UV_AfterCreateStatement(uv_work_t* req, int status) {
  DEBUG_PRINTF("ODBCConnection::UV_AfterCreateStatement\n");
  Nan::HandleScope scope;

  create_statement_work_data* data = (create_statement_work_data *)(req->data);

  DEBUG_PRINTF("ODBCConnection::UV_AfterCreateStatement\n");
  //DEBUG_PRINTF("ODBCConnection::UV_AfterCreateStatement m_hDBC=%X m_hDBC=%X hSTMT=%X\n",
  //  data->conn->m_hENV,
  //  data->conn->m_hDBC,
  //  data->hSTMT
  //);
  
  Local<Value> info[3];
  info[0] = Nan::New<External>(data->conn->m_hENV);
  info[1] = Nan::New<External>(data->conn->m_hDBC);
  info[2] = Nan::New<External>(data->hSTMT);
  
  Local<Object> js_result = Nan::NewInstance(Nan::New(ODBCStatement::constructor), 3, info).ToLocalChecked();

  info[0] = Nan::Null();
  info[1] = js_result;

  Nan::TryCatch try_catch;

  Nan::Call(*data->cb, 2, info);

  if (try_catch.HasCaught()) {
    Nan::FatalException(try_catch);
  }
  
  data->conn->Unref();
  delete data->cb;

  free(data);
  free(req);
}

/*
 * Query
 */

NAN_METHOD(ODBCConnection::Query) {
  DEBUG_PRINTF("ODBCConnection::Query\n");
  Nan::HandleScope scope;
  
  Local<Function> cb;
  
  Local<String> sql;
  
  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
  
  uv_work_t* work_req = (uv_work_t *) (calloc(1, sizeof(uv_work_t)));
  
  query_work_data* data = (query_work_data *) calloc(1, sizeof(query_work_data));

  //Check arguments for different variations of calling this function
  if (info.Length() == 3) {
    //handle Query("sql string", [params], function cb () {});
    
    if ( !info[0]->IsString() ) {
      return Nan::ThrowTypeError("Argument 0 must be an String.");
    }
    else if ( !info[1]->IsArray() ) {
      return Nan::ThrowTypeError("Argument 1 must be an Array.");
    }
    else if ( !info[2]->IsFunction() ) {
      return Nan::ThrowTypeError("Argument 2 must be a Function.");
    }

    sql = Local<String>::Cast(info[0]);
    
    data->params = ODBC::GetParametersFromArray(
      Local<Array>::Cast(info[1]),
      &data->paramCount,
      info.GetIsolate());
    
    cb = Local<Function>::Cast(info[2]);
  }
  else if (info.Length() == 2 ) {
    //handle either Query("sql", cb) or Query({ settings }, cb)
    
    if (!info[1]->IsFunction()) {
      return Nan::ThrowTypeError("ODBCConnection::Query(): Argument 1 must be a Function.");
    }
    
    cb = Local<Function>::Cast(info[1]);
    
    if (info[0]->IsString()) {
      //handle Query("sql", function cb () {})
      
      sql = Local<String>::Cast(info[0]);
      
      data->paramCount = 0;
    }
    else if (info[0]->IsObject()) {
      //NOTE: going forward this is the way we should expand options
      //rather than adding more arguments to the function signature.
      //specify options on an options object.
      //handle Query({}, function cb () {});
      
      Local<Object> obj = Local<Object>::Cast(info[0]);
      
      Local<String> optionSqlKey = Nan::New(OPTION_SQL);
      if (Nan::Has(obj, optionSqlKey).FromJust() && Nan::Get(obj, optionSqlKey).ToLocalChecked()->IsString()) {
        sql = Local<String>::Cast(Nan::Get(obj, optionSqlKey).ToLocalChecked());
      }
      else {
        sql = Nan::New("").ToLocalChecked();
      }
      
      Local<String> optionParamsKey = Nan::New(OPTION_PARAMS);
      if (Nan::Has(obj, optionParamsKey).FromJust() && Nan::Get(obj, optionParamsKey).ToLocalChecked()->IsArray()) {
        data->params = ODBC::GetParametersFromArray(
          Local<Array>::Cast(Nan::Get(obj, optionParamsKey).ToLocalChecked()),
          &data->paramCount,
          info.GetIsolate());
      }
      else {
        data->paramCount = 0;
      }
      
      Local<String> optionNoResultsKey = Nan::New(OPTION_NORESULTS);
      if (Nan::Has(obj, optionNoResultsKey).FromJust() && Nan::Get(obj, optionNoResultsKey).ToLocalChecked()->IsBoolean()) {
        data->noResultObject = Local<Boolean>::Cast(Nan::Get(obj, optionNoResultsKey).ToLocalChecked())->Value();
      }
      else {
        data->noResultObject = false;
      }
    }
    else {
      return Nan::ThrowTypeError("ODBCConnection::Query(): Argument 0 must be a String or an Object.");
    }
  }
  else {
    return Nan::ThrowTypeError("ODBCConnection::Query(): Requires either 2 or 3 Arguments. ");
  }
  //Done checking arguments

  data->cb = new Nan::Callback(cb);

#ifdef UNICODE
  data->sqlLen = sql->Length();
  data->sqlSize = (data->sqlLen * sizeof(uint16_t)) + sizeof(uint16_t);
  data->sql = (uint16_t *) malloc(data->sqlSize);
  STRING_WRITE(sql, info.GetIsolate(), (uint16_t *) data->sql);
#else
  data->sqlLen = sql->Utf8Length();
  data->sqlSize = data->sqlLen + 1;
  data->sql = (char *) malloc(data->sqlSize);
  STRING_WRITEUTF8(sql, info.GetIsolate(), (char *) data->sql);
#endif

  DEBUG_PRINTF("ODBCConnection::Query : sqlLen=%i, sqlSize=%i, sql=%s\n",
               data->sqlLen, data->sqlSize, (char*) data->sql);
  
  data->conn = conn;
  work_req->data = data;
  
  uv_queue_work(
    uv_default_loop(),
    work_req, 
    UV_Query, 
    (uv_after_work_cb)UV_AfterQuery);

  conn->Ref();

  info.GetReturnValue().Set(Nan::Undefined());
}

void ODBCConnection::UV_Query(uv_work_t* req) {
  DEBUG_PRINTF("ODBCConnection::UV_Query\n");
  
  query_work_data* data = (query_work_data *)(req->data);
  
  Parameter prm;
  SQLRETURN ret;
  
  uv_mutex_lock(&ODBC::g_odbcMutex);

  //allocate a new statment handle
  SQLAllocHandle( SQL_HANDLE_STMT, 
                  data->conn->m_hDBC, 
                  &data->hSTMT );

  uv_mutex_unlock(&ODBC::g_odbcMutex);

  // SQLExecDirect will use bound parameters, but without the overhead of SQLPrepare
  // for a single execution.
  if (data->paramCount) {
    for (int i = 0; i < data->paramCount; i++) {
      prm = data->params[i];


      /*DEBUG_TPRINTF(
        SQL_T("ODBCConnection::UV_Query - param[%i]: ValueType=%i type=%i BufferLength=%i size=%i length=%i &length=%X\n"), i, prm.ValueType, prm.ParameterType,
        prm.BufferLength, prm.ColumnSize, prm.length, &data->params[i].length);*/

      ret = SQLBindParameter(
        data->hSTMT,                        //StatementHandle
        i + 1,                              //ParameterNumber
        SQL_PARAM_INPUT,                    //InputOutputType
        prm.ValueType,
        prm.ParameterType,
        prm.ColumnSize,
        prm.DecimalDigits,
        prm.ParameterValuePtr,
        prm.BufferLength,
        &data->params[i].StrLen_or_IndPtr);

      if (ret == SQL_ERROR) {
        data->result = ret;
        return;
      }
    }
  }

  // execute the query directly
  ret = SQLExecDirect(
    data->hSTMT,
    (SQLTCHAR *)data->sql,
    data->sqlLen);

  // this will be checked later in UV_AfterQuery
  data->result = ret;
}

void ODBCConnection::UV_AfterQuery(uv_work_t* req, int status) {
  DEBUG_PRINTF("ODBCConnection::UV_AfterQuery\n");
  
  Nan::HandleScope scope;
  
  query_work_data* data = (query_work_data *)(req->data);

  Nan::TryCatch try_catch;

  DEBUG_PRINTF("ODBCConnection::UV_AfterQuery : data->result=%i, data->noResultObject=%i\n", data->result, data->noResultObject);

  if (data->result != SQL_ERROR && data->noResultObject) {
    //We have been requested to not create a result object
    //this means we should release the handle now and call back
    //with Nan::True()
    
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    SQLFreeHandle(SQL_HANDLE_STMT, data->hSTMT);
   
    uv_mutex_unlock(&ODBC::g_odbcMutex);
    
    Local<Value> info[2];
    info[0] = Nan::Null();
    info[1] = Nan::True();
    
    Nan::Call(*data->cb, 2, info);
  }
  else {
    Local<Value> info[4];
    bool* canFreeHandle = new bool(true);
    
    info[0] = Nan::New<External>(data->conn->m_hENV);
    info[1] = Nan::New<External>(data->conn->m_hDBC);
    info[2] = Nan::New<External>(data->hSTMT);
    info[3] = Nan::New<External>(canFreeHandle);
    
    Local<Object> js_result = Nan::NewInstance(Nan::New(ODBCResult::constructor), 4, info).ToLocalChecked();

    // Check now to see if there was an error (as there may be further result sets)
    if (data->result == SQL_ERROR) {
      info[0] = ODBC::GetSQLError(SQL_HANDLE_STMT, data->hSTMT, (char *) "[node-odbc] SQL_ERROR");
    } else {
        info[0] = Nan::Null();
    }
    info[1] = js_result;
    
    Nan::Call(*data->cb, 2, info);
  }
  
  data->conn->Unref();
  
  if (try_catch.HasCaught()) {
    Nan::FatalException(try_catch);
  }
  
  delete data->cb;

  if (data->paramCount) {
    Parameter prm;
    // free parameters
    for (int i = 0; i < data->paramCount; i++) {
      if (prm = data->params[i], prm.ParameterValuePtr != NULL) {
        switch (prm.ValueType) {
          case SQL_C_WCHAR:   free(prm.ParameterValuePtr);             break; 
          case SQL_C_CHAR:    free(prm.ParameterValuePtr);             break; 
          case SQL_C_LONG:    delete (int64_t *)prm.ParameterValuePtr; break;
          case SQL_C_DOUBLE:  delete (double  *)prm.ParameterValuePtr; break;
          case SQL_C_BIT:     delete (bool    *)prm.ParameterValuePtr; break;
        }
      }
    }
    
    free(data->params);
  }
  
  free(data->sql);
  free(data->catalog);
  free(data->schema);
  free(data->table);
  free(data->type);
  free(data->column);
  free(data);
  free(req);
}


/*
 * QuerySync
 */

NAN_METHOD(ODBCConnection::QuerySync) {
  DEBUG_PRINTF("ODBCConnection::QuerySync\n");
  Nan::HandleScope scope;

  Local<String> sql;

  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
  
  Parameter* params = new Parameter[0];
  Parameter prm;
  SQLRETURN ret;
  HSTMT hSTMT;
  int paramCount = 0;
  bool noResultObject = false;
  
  //Check arguments for different variations of calling this function
  if (info.Length() == 2) {
    //handle QuerySync("sql string", [params]);
    
    if ( !info[0]->IsString() ) {
      return Nan::ThrowTypeError("ODBCConnection::QuerySync(): Argument 0 must be an String.");
    }
    else if (!info[1]->IsArray()) {
      return Nan::ThrowTypeError("ODBCConnection::QuerySync(): Argument 1 must be an Array.");
    }

    sql = Local<String>::Cast(info[0]);

    params = ODBC::GetParametersFromArray(
      Local<Array>::Cast(info[1]),
      &paramCount,
      info.GetIsolate());

  }
  else if (info.Length() == 1 ) {
    //handle either QuerySync("sql") or QuerySync({ settings })

    if (info[0]->IsString()) {
      //handle Query("sql")
      sql = Local<String>::Cast(info[0]);
    
      paramCount = 0;
    }
    else if (info[0]->IsObject()) {
      //NOTE: going forward this is the way we should expand options
      //rather than adding more arguments to the function signature.
      //specify options on an options object.
      //handle Query({}, function cb () {});
      
      Local<Object> obj = Local<Object>::Cast(info[0]);
      
      Local<String> optionSqlKey = Nan::New<String>(OPTION_SQL);
      if (Nan::Has(obj, optionSqlKey).FromJust() && Nan::Get(obj, optionSqlKey).ToLocalChecked()->IsString()) {
        sql = Local<String>::Cast(Nan::Get(obj, optionSqlKey).ToLocalChecked());
      }
      else {
        sql = Nan::New("").ToLocalChecked();
      }

      Local<String> optionParamsKey = Nan::New(OPTION_PARAMS);
      if (Nan::Has(obj, optionParamsKey).FromJust() && Nan::Get(obj, optionParamsKey).ToLocalChecked()->IsArray()) {
        params = ODBC::GetParametersFromArray(
          Local<Array>::Cast(Nan::Get(obj, optionParamsKey).ToLocalChecked()),
          &paramCount,
          info.GetIsolate());
      }
      else {
        paramCount = 0;
      }
      
      Local<String> optionNoResultsKey = Nan::New(OPTION_NORESULTS);
      if (Nan::Has(obj, optionNoResultsKey).FromJust() && Nan::Get(obj, optionNoResultsKey).ToLocalChecked()->IsBoolean()) {
        noResultObject = Local<Boolean>::Cast(Nan::Get(obj, optionNoResultsKey).ToLocalChecked())->Value();
      }
    }
    else {
      return Nan::ThrowTypeError("ODBCConnection::QuerySync(): Argument 0 must be a String or an Object.");
    }
  }
  else {
    return Nan::ThrowTypeError("ODBCConnection::QuerySync(): Requires either 1 or 2 Arguments.");
  }
  //Done checking arguments

  uv_mutex_lock(&ODBC::g_odbcMutex);

  //allocate a new statment handle
  ret = SQLAllocHandle( SQL_HANDLE_STMT, 
                  conn->m_hDBC, 
                  &hSTMT );

  uv_mutex_unlock(&ODBC::g_odbcMutex);

  DEBUG_PRINTF("ODBCConnection::QuerySync - hSTMT=%p\n", hSTMT);
  
  if (SQL_SUCCEEDED(ret)) {
    if (paramCount) {
      DEBUG_PRINTF("ODBCConnection::QuerySync - binding params\n");
      for (int i = 0; i < paramCount; i++) {
        prm = params[i];
        
        DEBUG_PRINTF(
          "ODBCConnection::UV_Query - param[%i]: ValueType=%i type=%i BufferLength=%lli size=%lli length=%lli &length=%lli\n", i, prm.ValueType, prm.ParameterType, 
          prm.BufferLength, prm.ColumnSize, prm.StrLen_or_IndPtr, params[i].StrLen_or_IndPtr);

        ret = SQLBindParameter(
          hSTMT,                    //StatementHandle
          i + 1,                    //ParameterNumber
          SQL_PARAM_INPUT,          //InputOutputType
          prm.ValueType,
          prm.ParameterType,
          prm.ColumnSize,
          prm.DecimalDigits,
          prm.ParameterValuePtr,
          prm.BufferLength,
          &params[i].StrLen_or_IndPtr);
        
        if (ret == SQL_ERROR) {break;}
      }
    }

    // #ifdef UNICODE
      int sqlLen = sql->Length();
      int sqlSize = (sqlLen * sizeof(uint16_t)) + sizeof(uint16_t);

      uint16_t* sqlSubmit = (uint16_t *) malloc(sqlSize);
      STRING_WRITE(sql, info.GetIsolate(), (uint16_t *) sqlSubmit);
    // #else
      // int sqlLen = sql->Utf8Length(info.GetIsolate());
      // int sqlSize = sqlLen + 1;
      // char* sqlSubmit = (char *) malloc(sqlSize);
      // STRING_WRITEUTF8(sql, info.GetIsolate(), (char *) sqlSubmit);
    // #endif

    if (SQL_SUCCEEDED(ret)) {
      ret = SQLExecDirect(
        hSTMT,
        (SQLTCHAR* ) sqlSubmit,
        sqlLen);
    }
    
    // free parameters
    for (int i = 0; i < paramCount; i++) {
      if (prm = params[i], prm.ParameterValuePtr != NULL) {
        switch (prm.ValueType) {
          case SQL_C_WCHAR:   free(prm.ParameterValuePtr);             break;
          case SQL_C_CHAR:    free(prm.ParameterValuePtr);             break; 
          case SQL_C_LONG:    delete (int64_t *)prm.ParameterValuePtr; break;
          case SQL_C_DOUBLE:  delete (double  *)prm.ParameterValuePtr; break;
          case SQL_C_BIT:     delete (bool    *)prm.ParameterValuePtr; break;
        }
      }
    }
    
    free(params);
    free(sqlSubmit);
  }
  
  //check to see if there was an error during execution
  if (ret == SQL_ERROR) {
    Local<Value> objError = ODBC::GetSQLError(
      SQL_HANDLE_STMT,
      hSTMT,
      (char *) "[node-odbc] Error in ODBCConnection::QuerySync"
    );

    Nan::ThrowError(objError);
    
    return;
  }
  else if (noResultObject) {
    //if there is not result object requested then
    //we must destroy the STMT ourselves.
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    SQLFreeHandle(SQL_HANDLE_STMT, hSTMT);
   
    uv_mutex_unlock(&ODBC::g_odbcMutex);
    
    info.GetReturnValue().Set(Nan::True());
  }
  else {
    Local<Value> result[4];
    bool* canFreeHandle = new bool(true);
    
    result[0] = Nan::New<External>(conn->m_hENV);
    result[1] = Nan::New<External>(conn->m_hDBC);
    result[2] = Nan::New<External>(hSTMT);
    result[3] = Nan::New<External>(canFreeHandle);
    
    Local<Object> js_result = Nan::NewInstance(Nan::New(ODBCResult::constructor), 4, result).ToLocalChecked();

    info.GetReturnValue().Set(js_result);
  }
}


/*
 * GetInfoSync
 */

NAN_METHOD(ODBCConnection::GetInfoSync) {
  DEBUG_PRINTF("ODBCConnection::GetInfoSync\n");
  Nan::HandleScope scope;

  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());

  if (info.Length() == 1) {
    if ( !info[0]->IsNumber() ) {
        return Nan::ThrowTypeError("ODBCConnection::GetInfoSync(): Argument 0 must be a Number.");
    }
  }
  else {
    return Nan::ThrowTypeError("ODBCConnection::GetInfoSync(): Requires 1 Argument.");
  }

  SQLUSMALLINT InfoType = Local<Number>::Cast(info[0])->Value();

  switch (InfoType) {
    case SQL_USER_NAME:
      SQLRETURN ret;
      SQLTCHAR userName[255];
      SQLSMALLINT userNameLength;

      ret = SQLGetInfo(conn->m_hDBC, SQL_USER_NAME, userName, sizeof(userName), &userNameLength);

      if (SQL_SUCCEEDED(ret)) {
#ifdef UNICODE
        info.GetReturnValue().Set(Nan::New((uint16_t *)userName).ToLocalChecked());
#else
        info.GetReturnValue().Set(Nan::New((const char *) userName).ToLocalChecked());
#endif
      }
      break;

    default:
      return Nan::ThrowTypeError("ODBCConnection::GetInfoSync(): The only supported Argument is SQL_USER_NAME.");
  }
}


/*
 * Tables
 */

NAN_METHOD(ODBCConnection::Tables) {
  Nan::HandleScope scope;

  REQ_STRO_OR_NULL_ARG(0, catalog);
  REQ_STRO_OR_NULL_ARG(1, schema);
  REQ_STRO_OR_NULL_ARG(2, table);
  REQ_STRO_OR_NULL_ARG(3, type);
  Local<Function> cb = Local<Function>::Cast(info[4]);

  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
  
  uv_work_t* work_req = (uv_work_t *) (calloc(1, sizeof(uv_work_t)));
  
  query_work_data* data = 
    (query_work_data *) calloc(1, sizeof(query_work_data));
  
  if (!data) {
    Nan::LowMemoryNotification();
    Nan::ThrowError("Could not allocate enough memory");
    return;
  }

  data->sql = NULL;
  data->catalog = NULL;
  data->schema = NULL;
  data->table = NULL;
  data->type = NULL;
  data->column = NULL;
  data->cb = new Nan::Callback(cb);

  if (!Nan::Equals(catalog, Nan::Null()).FromJust()) {
#ifdef UNICODE
    data->catalog = (uint16_t *) malloc((catalog->Length() * sizeof(uint16_t)) + sizeof(uint16_t));
    STRING_WRITE(catalog, info.GetIsolate(), (uint16_t *) data->catalog);
#else
    data->catalog = (char *) malloc(catalog->Utf8Length() + 1);
    STRING_WRITEUTF8(catalog, info.GetIsolate(), (char *) data->catalog);
#endif
  }

  if (!Nan::Equals(schema, Nan::Null()).FromJust()) {
#ifdef UNICODE
    data->schema = (uint16_t *) malloc((schema->Length() * sizeof(uint16_t)) + sizeof(uint16_t));
    STRING_WRITE(schema, info.GetIsolate(), (uint16_t *) data->schema);
#else
    data->schema = (char *) malloc(schema->Utf8Length() + 1);
    STRING_WRITEUTF8(schema, info.GetIsolate(), (char *) data->schema);
#endif
  }
  
  if (!Nan::Equals(table, Nan::Null()).FromJust()) {
#ifdef UNICODE
    data->table = (uint16_t *) malloc((table->Length() * sizeof(uint16_t)) + sizeof(uint16_t));
    STRING_WRITE(table, info.GetIsolate(), (uint16_t *) data->table);
#else
    data->table = (char *) malloc(table->Utf8Length() + 1);
    STRING_WRITEUTF8(table, info.GetIsolate(), (char *) data->table);
#endif
  }
  
  if (!Nan::Equals(type, Nan::Null()).FromJust()) {
#ifdef UNICODE
    data->type = (uint16_t *) malloc((type->Length() * sizeof(uint16_t)) + sizeof(uint16_t));
    STRING_WRITE(type, info.GetIsolate(), (uint16_t *) data->type);
#else
    data->type = (char *) malloc(type->Utf8Length() + 1);
    STRING_WRITEUTF8(type, info.GetIsolate(), (char *) data->type);
#endif
  }
  
  data->conn = conn;
  work_req->data = data;
  
  uv_queue_work(
    uv_default_loop(), 
    work_req, 
    UV_Tables, 
    (uv_after_work_cb) UV_AfterQuery);

  conn->Ref();

  info.GetReturnValue().Set(Nan::Undefined());
}

void ODBCConnection::UV_Tables(uv_work_t* req) {
  query_work_data* data = (query_work_data *)(req->data);
  
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  SQLAllocHandle(SQL_HANDLE_STMT, data->conn->m_hDBC, &data->hSTMT );
  
  uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  SQLRETURN ret = SQLTables( 
    data->hSTMT, 
    (SQLTCHAR *) data->catalog,   SQL_NTS, 
    (SQLTCHAR *) data->schema,   SQL_NTS, 
    (SQLTCHAR *) data->table,   SQL_NTS, 
    (SQLTCHAR *) data->type,   SQL_NTS
  );
  
  // this will be checked later in UV_AfterQuery
  data->result = ret; 
}



/*
 * Columns
 */

NAN_METHOD(ODBCConnection::Columns) {
  Nan::HandleScope scope;

  REQ_STRO_OR_NULL_ARG(0, catalog);
  REQ_STRO_OR_NULL_ARG(1, schema);
  REQ_STRO_OR_NULL_ARG(2, table);
  REQ_STRO_OR_NULL_ARG(3, column);
  
  Local<Function> cb = Local<Function>::Cast(info[4]);
  
  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
  
  uv_work_t* work_req = (uv_work_t *) (calloc(1, sizeof(uv_work_t)));
  
  query_work_data* data = (query_work_data *) calloc(1, sizeof(query_work_data));
  
  if (!data) {
    Nan::LowMemoryNotification();
    Nan::ThrowError("Could not allocate enough memory");
    return;
  }

  data->sql = NULL;
  data->catalog = NULL;
  data->schema = NULL;
  data->table = NULL;
  data->type = NULL;
  data->column = NULL;
  data->cb = new Nan::Callback(cb);

  if (!Nan::Equals(catalog, Nan::Null()).FromJust()) {
#ifdef UNICODE
    data->catalog = (uint16_t *) malloc((catalog->Length() * sizeof(uint16_t)) + sizeof(uint16_t));
    STRING_WRITE(catalog, info.GetIsolate(), (uint16_t *) data->catalog);
#else
    data->catalog = (char *) malloc(catalog->Utf8Length() + 1);
    STRING_WRITEUTF8(catalog, info.GetIsolate(), (char *) data->catalog);
#endif
  }

  if (!Nan::Equals(schema, Nan::Null()).FromJust()) {
#ifdef UNICODE
    data->schema = (uint16_t *) malloc((schema->Length() * sizeof(uint16_t)) + sizeof(uint16_t));
    STRING_WRITE(schema, info.GetIsolate(), (uint16_t *) data->schema);
#else
    data->schema = (char *) malloc(schema->Utf8Length() + 1);
    STRING_WRITEUTF8(schema, info.GetIsolate(), (char *) data->schema);
#endif
  }
  
  if (!Nan::Equals(table, Nan::Null()).FromJust()) {
#ifdef UNICODE
    data->table = (uint16_t *) malloc((table->Length() * sizeof(uint16_t)) + sizeof(uint16_t));
    STRING_WRITE(table, info.GetIsolate(), (uint16_t *) data->table);
#else
    data->table = (char *) malloc(table->Utf8Length() + 1);
    STRING_WRITEUTF8(table, info.GetIsolate(), (char *) data->table);
#endif
  }
  
  if (!Nan::Equals(column, Nan::Null()).FromJust()) {
#ifdef UNICODE
    data->column = (uint16_t *) malloc((column->Length() * sizeof(uint16_t)) + sizeof(uint16_t));
    STRING_WRITE(column, info.GetIsolate(), (uint16_t *) data->column);
#else
    data->column = (char *) malloc(column->Utf8Length() + 1);
    STRING_WRITEUTF8(column, info.GetIsolate(), (char *) data->column);
#endif
  }
  
  data->conn = conn;
  work_req->data = data;
  
  uv_queue_work(
    uv_default_loop(),
    work_req, 
    UV_Columns, 
    (uv_after_work_cb)UV_AfterQuery);
  
  conn->Ref();

  info.GetReturnValue().Set(Nan::Undefined());
}

void ODBCConnection::UV_Columns(uv_work_t* req) {
  query_work_data* data = (query_work_data *)(req->data);
  
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  SQLAllocHandle(SQL_HANDLE_STMT, data->conn->m_hDBC, &data->hSTMT );
  
  uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  SQLRETURN ret = SQLColumns( 
    data->hSTMT, 
    (SQLTCHAR *) data->catalog,   SQL_NTS, 
    (SQLTCHAR *) data->schema,   SQL_NTS, 
    (SQLTCHAR *) data->table,   SQL_NTS, 
    (SQLTCHAR *) data->column,   SQL_NTS
  );
  
  // this will be checked later in UV_AfterQuery
  data->result = ret;
}

/*
 * BeginTransactionSync
 * 
 */

NAN_METHOD(ODBCConnection::BeginTransactionSync) {
  DEBUG_PRINTF("ODBCConnection::BeginTransactionSync\n");
  Nan::HandleScope scope;

  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
  
  SQLRETURN ret;

  //set the connection manual commits
  ret = SQLSetConnectAttr(
    conn->m_hDBC,
    SQL_ATTR_AUTOCOMMIT,
    (SQLPOINTER) SQL_AUTOCOMMIT_OFF,
    SQL_NTS);
  
  if (!SQL_SUCCEEDED(ret)) {
    Local<Value> objError = ODBC::GetSQLError(SQL_HANDLE_DBC, conn->m_hDBC);
    
    Nan::ThrowError(objError);
    
    info.GetReturnValue().Set(Nan::False());
  }
  
  info.GetReturnValue().Set(Nan::True());
}

/*
 * BeginTransaction
 * 
 */

NAN_METHOD(ODBCConnection::BeginTransaction) {
  DEBUG_PRINTF("ODBCConnection::BeginTransaction\n");
  Nan::HandleScope scope;

  REQ_FUN_ARG(0, cb);

  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
  
  uv_work_t* work_req = (uv_work_t *) (calloc(1, sizeof(uv_work_t)));
  
  query_work_data* data = 
    (query_work_data *) calloc(1, sizeof(query_work_data));
  
  if (!data) {
    Nan::LowMemoryNotification();
    return Nan::ThrowError("Could not allocate enough memory");
  }

  data->cb = new Nan::Callback(cb);
  data->conn = conn;
  work_req->data = data;
  
  uv_queue_work(
    uv_default_loop(),
    work_req, 
    UV_BeginTransaction, 
    (uv_after_work_cb)UV_AfterBeginTransaction);

  return;
}

/*
 * UV_BeginTransaction
 * 
 */

void ODBCConnection::UV_BeginTransaction(uv_work_t* req) {
  DEBUG_PRINTF("ODBCConnection::UV_BeginTransaction\n");
  
  query_work_data* data = (query_work_data *)(req->data);
  
  //set the connection manual commits
  data->result = SQLSetConnectAttr(
    data->conn->self()->m_hDBC,
    SQL_ATTR_AUTOCOMMIT,
    (SQLPOINTER) SQL_AUTOCOMMIT_OFF,
    SQL_NTS);
}

/*
 * UV_AfterBeginTransaction
 * 
 */

void ODBCConnection::UV_AfterBeginTransaction(uv_work_t* req, int status) {
  DEBUG_PRINTF("ODBCConnection::UV_AfterBeginTransaction\n");
  Nan::HandleScope scope;

  //TODO: Is this supposed to be of type query_work_data?
  open_connection_work_data* data = (open_connection_work_data *)(req->data);
  
  Local<Value> argv[1];
  
  bool err = false;

  if (!SQL_SUCCEEDED(data->result)) {
    err = true;

    Local<Object> objError = ODBC::GetSQLError(SQL_HANDLE_DBC, data->conn->self()->m_hDBC);
    
    argv[0] = objError;
  }

  Nan::TryCatch try_catch;

  Nan::Call(*data->cb, err ? 1 : 0, argv);

  if (try_catch.HasCaught()) {
    Nan::FatalException(try_catch);
  }

  delete data->cb;
  
  free(data);
  free(req);
}

/*
 * EndTransactionSync
 * 
 */

NAN_METHOD(ODBCConnection::EndTransactionSync) {
  DEBUG_PRINTF("ODBCConnection::EndTransactionSync\n");
  Nan::HandleScope scope;

  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
  
  REQ_BOOL_ARG(0, rollback);
  
  Local<Value> objError;
  SQLRETURN ret;
  bool error = false;
  SQLSMALLINT completionType = (rollback->Value()) 
    ? SQL_ROLLBACK
    : SQL_COMMIT
    ;
  
  //Call SQLEndTran
  ret = SQLEndTran(
    SQL_HANDLE_DBC,
    conn->m_hDBC,
    completionType);
  
  //check how the transaction went
  if (!SQL_SUCCEEDED(ret)) {
    error = true;
    
    objError = ODBC::GetSQLError(SQL_HANDLE_DBC, conn->m_hDBC);
  }
  
  //Reset the connection back to autocommit
  ret = SQLSetConnectAttr(
    conn->m_hDBC,
    SQL_ATTR_AUTOCOMMIT,
    (SQLPOINTER) SQL_AUTOCOMMIT_ON,
    SQL_NTS);
  
  //check how setting the connection attr went
  //but only process the code if an error has not already
  //occurred. If an error occurred during SQLEndTran,
  //that is the error that we want to throw.
  if (!SQL_SUCCEEDED(ret) && !error) {
    //TODO: if this also failed, we really should
    //be restarting the connection or something to deal with this state
    error = true;
    
    objError = ODBC::GetSQLError(SQL_HANDLE_DBC, conn->m_hDBC);
  }
  
  if (error) {
    Nan::ThrowError(objError);
    
    info.GetReturnValue().Set(Nan::False());
  }
  else {
    info.GetReturnValue().Set(Nan::True());
  }
}

/*
 * EndTransaction
 * 
 */

NAN_METHOD(ODBCConnection::EndTransaction) {
  DEBUG_PRINTF("ODBCConnection::EndTransaction\n");
  Nan::HandleScope scope;

  REQ_BOOL_ARG(0, rollback);
  REQ_FUN_ARG(1, cb);

  ODBCConnection* conn = Nan::ObjectWrap::Unwrap<ODBCConnection>(info.Holder());
  
  uv_work_t* work_req = (uv_work_t *) (calloc(1, sizeof(uv_work_t)));
  
  query_work_data* data = 
    (query_work_data *) calloc(1, sizeof(query_work_data));
  
  if (!data) {
    Nan::LowMemoryNotification();
    return Nan::ThrowError("Could not allocate enough memory");
  }
  
  data->completionType = (rollback->Value()) 
    ? SQL_ROLLBACK
    : SQL_COMMIT
    ;
  data->cb = new Nan::Callback(cb);
  data->conn = conn;
  work_req->data = data;
  
  uv_queue_work(
    uv_default_loop(),
    work_req, 
    UV_EndTransaction, 
    (uv_after_work_cb)UV_AfterEndTransaction);

  info.GetReturnValue().Set(Nan::Undefined());
}

/*
 * UV_EndTransaction
 * 
 */

void ODBCConnection::UV_EndTransaction(uv_work_t* req) {
  DEBUG_PRINTF("ODBCConnection::UV_EndTransaction\n");
  
  query_work_data* data = (query_work_data *)(req->data);
  
  bool err = false;
  
  //Call SQLEndTran
  SQLRETURN ret = SQLEndTran(
    SQL_HANDLE_DBC,
    data->conn->m_hDBC,
    data->completionType);
  
  data->result = ret;
  
  if (!SQL_SUCCEEDED(ret)) {
    err = true;
  }
  
  //Reset the connection back to autocommit
  ret = SQLSetConnectAttr(
    data->conn->m_hDBC,
    SQL_ATTR_AUTOCOMMIT,
    (SQLPOINTER) SQL_AUTOCOMMIT_ON,
    SQL_NTS);
  
  if (!SQL_SUCCEEDED(ret) && !err) {
    //there was not an earlier error,
    //so we shall pass the return code from
    //this last call.
    data->result = ret;
  }
}

/*
 * UV_AfterEndTransaction
 * 
 */

void ODBCConnection::UV_AfterEndTransaction(uv_work_t* req, int status) {
  DEBUG_PRINTF("ODBCConnection::UV_AfterEndTransaction\n");
  Nan::HandleScope scope;
  
  open_connection_work_data* data = (open_connection_work_data *)(req->data);
  
  Local<Value> argv[1];
  
  bool err = false;

  if (!SQL_SUCCEEDED(data->result)) {
    err = true;

    Local<Object> objError = ODBC::GetSQLError(SQL_HANDLE_DBC, data->conn->self()->m_hDBC);
    
    argv[0] = objError;
  }

  Nan::TryCatch try_catch;

  Nan::Call(*data->cb, err ? 1 : 0, argv);

  if (try_catch.HasCaught()) {
    Nan::FatalException(try_catch);
  }

  delete data->cb;
  
  free(data);
  free(req);
}
