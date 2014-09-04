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
#include <node_buffer.h>
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

using namespace v8;
using namespace node;

uv_mutex_t ODBC::g_odbcMutex;
uv_async_t ODBC::g_async;

Persistent<FunctionTemplate> ODBC::constructor_template;

void ODBC::Init(v8::Handle<Object> target) {
  DEBUG_PRINTF("ODBC::Init\n");
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  // Constructor Template
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->SetClassName(String::NewSymbol("ODBC"));

  // Reserve space for one Handle<Value>
  Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
  instance_template->SetInternalFieldCount(1);
  
  // Constants
  NODE_DEFINE_CONSTANT(constructor_template, SQL_CLOSE);
  NODE_DEFINE_CONSTANT(constructor_template, SQL_DROP);
  NODE_DEFINE_CONSTANT(constructor_template, SQL_UNBIND);
  NODE_DEFINE_CONSTANT(constructor_template, SQL_RESET_PARAMS);
  NODE_DEFINE_CONSTANT(constructor_template, SQL_DESTROY); //SQL_DESTROY is non-standard
  NODE_DEFINE_CONSTANT(constructor_template, FETCH_ARRAY);
  NODE_DEFINE_CONSTANT(constructor_template, FETCH_OBJECT);
  NODE_DEFINE_CONSTANT(constructor_template, FETCH_NONE);
  
  // Prototype Methods
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "createConnection", CreateConnection);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "createConnectionSync", CreateConnectionSync);

  // Attach the Database Constructor to the target object
  target->Set( v8::String::NewSymbol("ODBC"),
               constructor_template->GetFunction());
  
  scope.Close(Undefined());
  
#if NODE_VERSION_AT_LEAST(0, 7, 9)
  // Initialize uv_async so that we can prevent node from exiting
  uv_async_init( uv_default_loop(),
                 &ODBC::g_async,
                 ODBC::WatcherCallback);
  
  // Not sure if the init automatically calls uv_ref() because there is weird
  // behavior going on. When ODBC::Init is called which initializes the 
  // uv_async_t g_async above, there seems to be a ref which will keep it alive
  // but we only want this available so that we can uv_ref() later on when
  // we have a connection.
  // so to work around this, I am possibly mistakenly calling uv_unref() once
  // so that there are no references on the loop.
  uv_unref((uv_handle_t *)&ODBC::g_async);
#endif
  
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

Handle<Value> ODBC::New(const Arguments& args) {
  DEBUG_PRINTF("ODBC::New\n");
  HandleScope scope;
  ODBC* dbo = new ODBC();
  
  dbo->Wrap(args.Holder());
  dbo->m_hEnv = NULL;
  
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  int ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &dbo->m_hEnv);
  
  uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  if (!SQL_SUCCEEDED(ret)) {
    DEBUG_PRINTF("ODBC::New - ERROR ALLOCATING ENV HANDLE!!\n");
    
    Local<Object> objError = ODBC::GetSQLError(SQL_HANDLE_ENV, dbo->m_hEnv);
    
    ThrowException(objError);
  }
  
  SQLSetEnvAttr(dbo->m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, SQL_IS_UINTEGER);
  
  return scope.Close(args.Holder());
}

void ODBC::WatcherCallback(uv_async_t *w, int revents) {
  DEBUG_PRINTF("ODBC::WatcherCallback\n");
  //i don't know if we need to do anything here
}

/*
 * CreateConnection
 */

Handle<Value> ODBC::CreateConnection(const Arguments& args) {
  DEBUG_PRINTF("ODBC::CreateConnection\n");
  HandleScope scope;

  REQ_FUN_ARG(0, cb);

  ODBC* dbo = ObjectWrap::Unwrap<ODBC>(args.Holder());
  
  //initialize work request
  uv_work_t* work_req = (uv_work_t *) (calloc(1, sizeof(uv_work_t)));
  
  //initialize our data
  create_connection_work_data* data = 
    (create_connection_work_data *) (calloc(1, sizeof(create_connection_work_data)));

  data->cb = Persistent<Function>::New(cb);
  data->dbo = dbo;

  work_req->data = data;
  
  uv_queue_work(uv_default_loop(), work_req, UV_CreateConnection, (uv_after_work_cb)UV_AfterCreateConnection);

  dbo->Ref();

  return scope.Close(Undefined());
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
  HandleScope scope;

  create_connection_work_data* data = (create_connection_work_data *)(req->data);
  
  TryCatch try_catch;
  
  if (!SQL_SUCCEEDED(data->result)) {
    Local<Value> args[1];
    
    args[0] = ODBC::GetSQLError(SQL_HANDLE_ENV, data->dbo->m_hEnv);
    
    data->cb->Call(Context::GetCurrent()->Global(), 1, args);
  }
  else {
    Local<Value> args[2];
    args[0] = External::New(data->dbo->m_hEnv);
    args[1] = External::New(data->hDBC);
    
    Local<Object> js_result(ODBCConnection::constructor_template->
                              GetFunction()->NewInstance(2, args));

    args[0] = Local<Value>::New(Null());
    args[1] = Local<Object>::New(js_result);

    data->cb->Call(Context::GetCurrent()->Global(), 2, args);
  }
  
  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  
  data->dbo->Unref();
  data->cb.Dispose();

  free(data);
  free(req);
  
  scope.Close(Undefined());
}

/*
 * CreateConnectionSync
 */

Handle<Value> ODBC::CreateConnectionSync(const Arguments& args) {
  DEBUG_PRINTF("ODBC::CreateConnectionSync\n");
  HandleScope scope;

  ODBC* dbo = ObjectWrap::Unwrap<ODBC>(args.Holder());
   
  HDBC hDBC;
  
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  //allocate a new connection handle
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_DBC, dbo->m_hEnv, &hDBC);
  
  if (!SQL_SUCCEEDED(ret)) {
    //TODO: do something!
  }
  
  uv_mutex_unlock(&ODBC::g_odbcMutex);

  Local<Value> params[2];
  params[0] = External::New(dbo->m_hEnv);
  params[1] = External::New(hDBC);

  Local<Object> js_result(ODBCConnection::constructor_template->
                            GetFunction()->NewInstance(2, params));

  return scope.Close(js_result);
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
  DEBUG_PRINTF("ODBC::FreeColumns(0x%x, %i)\n", columns, (int)*colCount);

  for(int i = 0; i < *colCount; i++) {
      delete [] columns[i].name;
  }

  delete [] columns;
  
  *colCount = 0;
}

/*
 * GetColumnValue
 */

SQLSMALLINT ODBC::GetCColumnType(const Column& column) {
  switch ((int)column.type) {
    case SQL_INTEGER: case SQL_SMALLINT: case SQL_TINYINT:
      return SQL_C_SLONG;

    case SQL_NUMERIC: case SQL_DECIMAL: case SQL_BIGINT:
    case SQL_FLOAT: case SQL_REAL: case SQL_DOUBLE:
      return SQL_C_DOUBLE;

    // Date and time
    case SQL_TIMESTAMP: case SQL_TYPE_TIMESTAMP:
    case SQL_DATE: case SQL_TYPE_DATE:
      return SQL_C_TYPE_TIMESTAMP;

    case SQL_BINARY: case SQL_VARBINARY: case SQL_LONGVARBINARY:
      return SQL_C_BINARY;

    case SQL_BIT:
      return SQL_C_BIT;

    default:
      return SQL_C_TCHAR;
  }
}

SQLRETURN ODBC::GetColumnData( SQLHSTMT hStmt, const Column& column, 
                               void* buffer, SQLLEN bufferLength, SQLSMALLINT& cType, SQLLEN& len) {

  cType = GetCColumnType(column);

  switch(cType) {
    case SQL_C_SLONG:
      bufferLength = sizeof(long);
      break;
    
    case SQL_C_DOUBLE:
      bufferLength = sizeof(double);
      break;
  }

  return SQLGetData(
      hStmt,
      column.index,
      cType,
      buffer,
      bufferLength,
      &len);
}

Handle<Value> ODBC::ConvertColumnValue( SQLSMALLINT cType,
                                        uint16_t* buffer, SQLINTEGER bytesInBuffer,
                                        Buffer* resultBuffer, size_t resultBufferOffset) {

  HandleScope scope;

  switch (cType) {
    case SQL_C_SLONG:
      assert(bytesInBuffer >= sizeof(long));
      // XXX Integer::New will truncate here if sizeof(long) > 4, it expects 32-bit numbers
      return scope.Close(Integer::New(*reinterpret_cast<long*>(buffer)));
      break;

    case SQL_C_DOUBLE:
      assert(bytesInBuffer >= sizeof(double));
      return scope.Close(Number::New(*reinterpret_cast<double*>(buffer)));
      break;

    case SQL_C_TYPE_TIMESTAMP: {
      struct tm timeInfo = { 0 };
      SQLUINTEGER fraction = 0;

      assert(bytesInBuffer >= sizeof(SQL_TIMESTAMP_STRUCT));
      SQL_TIMESTAMP_STRUCT& odbcTime = *reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(buffer);

      timeInfo.tm_year = odbcTime.year - 1900;
      timeInfo.tm_mon = odbcTime.month - 1;
      timeInfo.tm_mday = odbcTime.day;
      timeInfo.tm_hour = odbcTime.hour;
      timeInfo.tm_min = odbcTime.minute;
      timeInfo.tm_sec = odbcTime.second;
      fraction = odbcTime.fraction;

      //a negative value means that mktime() should use timezone information 
      //and system databases to attempt to determine whether DST is in effect 
      //at the specified time.
      timeInfo.tm_isdst = -1;

#if defined(_WIN32) && defined (TIMEGM)
      return scope.Close(Date::New((double(_mkgmtime32(&timeInfo)) * 1000)
        + (fraction / 1000000.0)));
#elif defined(WIN32)
      return scope.Close(Date::New((double(mktime(&timeInfo)) * 1000)
        + (fraction / 1000000.0)));
#elif defined(TIMEGM)
      return scope.Close(Date::New((double(timegm(&timeInfo)) * 1000)
        + (fraction / 1000000.0)));
#else
      return scope.Close(Date::New((double(timelocal(&timeInfo)) * 1000)
        + (fraction / 1000000.0)));
#endif
      break;
    }

    case SQL_C_BIT:
      assert(bytesInBuffer >= sizeof(SQLCHAR));
      return scope.Close(Boolean::New(!!*reinterpret_cast<SQLCHAR*>(buffer)));

    default:
      assert(!"ODBC::ConvertColumnValue: Internal error (unexpected C type)");
  }
}

SQLRETURN ODBC::FetchMoreData( SQLHSTMT hStmt, const Column& column, SQLSMALLINT cType,
                               SQLLEN& bytesAvailable, SQLLEN& bytesRead,
                               void* internalBuffer, SQLLEN internalBufferLength,
                               void* resultBuffer, size_t& offset, SQLLEN resultBufferLength ) {

  bytesRead = 0;
  SQLRETURN ret;

  if (resultBuffer) {
    // Just use the node::Buffer we have to avoid memcpy()ing
    SQLLEN remainingBuffer = resultBufferLength - offset;
    ret = GetColumnData(hStmt, column, (char*)resultBuffer + offset, remainingBuffer, cType, bytesAvailable); 
    if (!SQL_SUCCEEDED(ret) || bytesAvailable == SQL_NULL_DATA)
      return ret;

    bytesRead = min(bytesAvailable, remainingBuffer);

    if (cType == SQL_C_TCHAR && bytesRead == remainingBuffer)
      bytesRead -= sizeof(TCHAR); // The last byte or two bytes of the buffer is actually a null terminator (gee, thanks, ODBC).

    offset += bytesRead;
          
    DEBUG_PRINTF("ODBC::FetchMoreData: SQLGetData(slowBuffer, bufferLength=%i): returned %i with %i bytes available\n", 
      remainingBuffer, ret, bytesAvailable);
  } else {
    // We don't know how much data there is to get back yet...
    // Use our buffer for now for the first SQLGetData call, which will tell us the full length.
    ret = GetColumnData(hStmt, column, internalBuffer, internalBufferLength, cType, bytesAvailable);

    if (bytesAvailable == SQL_NULL_DATA)
      bytesRead = bytesAvailable;

    if (!SQL_SUCCEEDED(ret) || bytesAvailable == SQL_NULL_DATA)
      return ret;

    bytesRead = min(bytesAvailable, internalBufferLength);

    if (cType == SQL_C_TCHAR && bytesRead == internalBufferLength)
      bytesRead -= sizeof(TCHAR); // The last byte or two bytes of the buffer is actually a null terminator (gee, thanks, ODBC).

    offset += bytesRead;

    DEBUG_PRINTF("ODBC::FetchMoreData: SQLGetData(internalBuffer, bufferLength=%i): returned %i with %i bytes available\n", 
      internalBufferLength, ret, bytesAvailable);

    // Now would be a good time to create the result buffer, but we can't call 
    // Buffer::New here if we are on a worker thread
  }

  return ret;
}

Handle<Value> ODBC::GetColumnValue( SQLHSTMT hStmt, Column column, 
                                    uint16_t* buffer, SQLLEN bufferLength,
                                    bool partial, bool fetch) {
  HandleScope scope;

  // Reset the buffer
  buffer[0] = '\0';

  if (!fetch)
    partial = true;

  DEBUG_PRINTF("ODBC::GetColumnValue: column=%s, type=%i, buffer=%x, bufferLength=%i, partial=%i, fetch=%i\n", 
    column.name, column.type, buffer, bufferLength, partial?1:0, fetch?1:0);

  SQLSMALLINT cType = GetCColumnType(column);

  // Fixed length column
  if (cType != SQL_C_BINARY && cType != SQL_C_TCHAR) {
    SQLLEN bytesAvailable = 0, bytesRead = 0;

    if (fetch) {
        // Use the ODBCResult's buffer
        // TODO For fixed-length columns, the driver will just assume the buffer is big enough
        //      Ensure this won't break anything.

        SQLRETURN ret = GetColumnData(hStmt, column, buffer, bufferLength, cType, bytesAvailable);
        if (!SQL_SUCCEEDED(ret))
          return ThrowException(ODBC::GetSQLError(SQL_HANDLE_STMT, hStmt));

        if (bytesAvailable == SQL_NULL_DATA)
          return scope.Close(Null());

        bytesRead = bytesAvailable > bufferLength ? bufferLength : bytesAvailable;
        DEBUG_PRINTF(" *** SQLGetData(internalBuffer, bufferLength=%i) for fixed type: returned %i with %i bytes available\n", bufferLength, ret, bytesAvailable);

        return scope.Close(ConvertColumnValue(cType, buffer, bytesRead, 0, 0));
    } else {
      return scope.Close(ConvertColumnValue(cType, buffer, bufferLength, 0, 0));
    }
  }
  
  // Varying length column - fetch piece by piece (in as few pieces as possible - fetch the first piece into the 
  // ODBCResult's buffer to find out how big the column is, then allocate a node::Buffer to hold the rest of it)
  Buffer* slowBuffer = 0;
  size_t offset = 0;
  
  SQLRETURN ret = 0; 
  SQLLEN bytesAvailable = 0, bytesRead = 0;

  do {
    if (fetch) {
      ret = FetchMoreData(
        hStmt, column, cType, 
        bytesAvailable, bytesRead, 
        buffer, bufferLength, 
        slowBuffer ? Buffer::Data(slowBuffer) : 0, offset, slowBuffer ? Buffer::Length(slowBuffer) : 0);
    } else {
      ret = SQL_SUCCESS;
      bytesAvailable = bufferLength;
      bytesRead = bufferLength;
    }

    // Maybe this should be an error?
    if (ret == SQL_NO_DATA)
      return scope.Close(Undefined());

    if (!SQL_SUCCEEDED(ret))
      return ThrowException(ODBC::GetSQLError(SQL_HANDLE_STMT, hStmt, "ODBC::GetColumnValue: Error getting data for result column"));
    
    if (slowBuffer)
      assert(offset <= Buffer::Length(slowBuffer));
 
    // The SlowBuffer is needed when 
    //  (a) the result type is Binary 
    //  (b) the result is a string longer than our internal buffer (and we hopefully know the length).
    // Move data from internal buffer to node::Buffer now.
    if (!slowBuffer && (ret == SQL_SUCCESS_WITH_INFO || cType == SQL_C_BINARY)) {
      slowBuffer = Buffer::New(bytesAvailable + (cType == SQL_C_TCHAR ? sizeof(TCHAR) : 0)); // Allow space for ODBC to add an unnecessary null terminator
      memcpy(Buffer::Data(slowBuffer), buffer, bytesRead);
      DEBUG_PRINTF(" *** Created new SlowBuffer (length: %i), copied %i bytes from internal buffer\n", Buffer::Length(slowBuffer), bytesRead);
    }

  // SQLGetData returns SQL_SUCCESS on the last chunk, SQL_SUCCESS_WITH_INFO and SQLSTATE 01004 on the preceding chunks.
  } while (!partial && ret == SQL_SUCCESS_WITH_INFO);

  return scope.Close(InterpretBuffers(cType, buffer, bytesRead, slowBuffer ? slowBuffer->handle_ : Handle<Object>(), slowBuffer, offset));
}

Handle<Value> ODBC::InterpretBuffers( SQLSMALLINT cType, 
                                      void* internalBuffer, SQLLEN bytesRead, 
                                      Handle<Object> resultBufferHandle, 
                                      void* resultBuffer, size_t resultBufferOffset) {
  DEBUG_PRINTF("ODBC::InterpretBuffers(%i, %p, %i, _, %p, %u)\n", cType, internalBuffer, bytesRead, resultBuffer, resultBufferOffset);

  HandleScope scope;
                                         
  switch (cType) {
    case SQL_C_BINARY: {
      // XXX Can we trust the global Buffer object? What if the caller has set it to something non-Function? Crash?
      Local<Function> bufferConstructor = Local<Function>::Cast(Context::GetCurrent()->Global()->Get(String::New("Buffer")));
      Handle<Value> constructorArgs[1] = { resultBufferHandle };
      return scope.Close(bufferConstructor->NewInstance(1, constructorArgs));
    }

    // TODO when UNICODE is not defined, assumes that the character encoding of the data received is UTF-8
    case SQL_C_TCHAR: default:
#ifdef UNICODE
      // slowBuffer may or may not exist, depending on whether or not the data had to be split
      if (resultBuffer)
        assert (resultBufferOffset % 2 == 0);
      if (resultBuffer)
        return scope.Close(String::New(reinterpret_cast<uint16_t*>(resultBuffer), resultBufferOffset / 2));
      else
        return scope.Close(String::New(reinterpret_cast<uint16_t*>(internalBuffer), bytesRead / 2));
#else
      if (resultBuffer)
        return scope.Close(String::New(reinterpret_cast<char*>(resultBuffer), resultBufferOffset));
      else
        return scope.Close(String::New(reinterpret_cast<char*>(internalBuffer), bytesRead));
#endif
  }
}

/*
 * GetRecordTuple
 */

Local<Object> ODBC::GetRecordTuple ( SQLHSTMT hStmt, Column* columns, 
                                     short* colCount, uint16_t* buffer,
                                     int bufferLength) {
  HandleScope scope;
  
  Local<Object> tuple = Object::New();
        
  for(int i = 0; i < *colCount; i++) {
    Handle<Value> value = GetColumnValue( hStmt, columns[i], buffer, bufferLength );
    if (value->IsUndefined())
      return scope.Close(Local<Object>());
#ifdef UNICODE
    tuple->Set( String::New((uint16_t *) columns[i].name), value );
#else
    tuple->Set( String::New((const char *) columns[i].name), value );
#endif
  }
  
  //return tuple;
  return scope.Close(tuple);
}

/*
 * GetRecordArray
 */

Handle<Value> ODBC::GetRecordArray ( SQLHSTMT hStmt, Column* columns, 
                                         short* colCount, uint16_t* buffer,
                                         int bufferLength) {
  HandleScope scope;
  
  Local<Array> array = Array::New();
        
  for(int i = 0; i < *colCount; i++) {
    Handle<Value> value = GetColumnValue( hStmt, columns[i], buffer, bufferLength );
    if (value->IsUndefined())
      return scope.Close(Handle<Value>());
    array->Set( Integer::New(i), value );
  }
  
  //return array;
  return scope.Close(array);
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
    
    params[i].size          = 0;
    params[i].length        = SQL_NULL_DATA;
    params[i].buffer_length = 0;
    params[i].decimals      = 0;

    DEBUG_PRINTF("ODBC::GetParametersFromArray - &param[%i].length = %X\n",
                 i, &params[i].length);

    if (value->IsString()) {
      Local<String> string = value->ToString();
      int length = string->Length();
      
      params[i].c_type        = SQL_C_TCHAR;
#ifdef UNICODE
      params[i].type          = SQL_WLONGVARCHAR;
      params[i].buffer_length = (length * sizeof(uint16_t)) + sizeof(uint16_t);
#else
      params[i].type          = SQL_LONGVARCHAR;
      params[i].buffer_length = string->Utf8Length() + 1;
#endif
      params[i].buffer        = malloc(params[i].buffer_length);
      params[i].size          = params[i].buffer_length;
      params[i].length        = SQL_NTS;//params[i].buffer_length;

#ifdef UNICODE
      string->Write((uint16_t *) params[i].buffer);
#else
      string->WriteUtf8((char *) params[i].buffer);
#endif

      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsString(): params[%i] "
                   L"c_type=%i type=%i buffer_length=%i size=%i length=%i "
                   L"value=%s\n", i, params[i].c_type, params[i].type,
                   params[i].buffer_length, params[i].size, params[i].length, 
                   (char*) params[i].buffer);
    }
    else if (value->IsNull()) {
      params[i].c_type = SQL_C_DEFAULT;
      params[i].type   = SQL_VARCHAR;
      params[i].length = SQL_NULL_DATA;

      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsNull(): params[%i] "
                   L"c_type=%i type=%i buffer_length=%i size=%i length=%i\n",
                   i, params[i].c_type, params[i].type,
                   params[i].buffer_length, params[i].size, params[i].length);
    }
    else if (value->IsInt32()) {
      int64_t  *number = new int64_t(value->IntegerValue());
      params[i].c_type = SQL_C_SBIGINT;
      params[i].type   = SQL_BIGINT;
      params[i].buffer = number;
      params[i].length = 0;
      
      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsInt32(): params[%i] "
                   L"c_type=%i type=%i buffer_length=%i size=%i length=%i "
                   L"value=%lld\n", i, params[i].c_type, params[i].type,
                   params[i].buffer_length, params[i].size, params[i].length,
                   *number);
    }
    else if (value->IsNumber()) {
      double *number   = new double(value->NumberValue());
      
      params[i].c_type        = SQL_C_DOUBLE;
      params[i].type          = SQL_DECIMAL;
      params[i].buffer        = number;
      params[i].buffer_length = sizeof(double);
      params[i].length        = params[i].buffer_length;
      params[i].decimals      = 7;
      params[i].size          = sizeof(double);

      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsNumber(): params[%i] "
                  L"c_type=%i type=%i buffer_length=%i size=%i length=%i "
		          L"value=%f\n",
                  i, params[i].c_type, params[i].type,
                  params[i].buffer_length, params[i].size, params[i].length,
		  *number);
    }
    else if (value->IsBoolean()) {
      bool *boolean    = new bool(value->BooleanValue());
      params[i].c_type = SQL_C_BIT;
      params[i].type   = SQL_BIT;
      params[i].buffer = boolean;
      params[i].length = 0;
      
      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsBoolean(): params[%i] "
                   L"c_type=%i type=%i buffer_length=%i size=%i length=%i\n",
                   i, params[i].c_type, params[i].type,
                   params[i].buffer_length, params[i].size, params[i].length);
    }
  } 
  
  return params;
}

/*
 * CallbackSQLError
 */

Handle<Value> ODBC::CallbackSQLError (SQLSMALLINT handleType,
                                      SQLHANDLE handle, 
                                      Persistent<Function> cb) {
  HandleScope scope;
  
  return scope.Close(CallbackSQLError(
    handleType,
    handle,
    (char *) "[node-odbc] SQL_ERROR",
    cb));
}

Handle<Value> ODBC::CallbackSQLError (SQLSMALLINT handleType,
                                      SQLHANDLE handle,
                                      char* message,
                                      Persistent<Function> cb) {
  HandleScope scope;
  
  Local<Object> objError = ODBC::GetSQLError(
    handleType, 
    handle, 
    message
  );
  
  Local<Value> args[1];
  args[0] = objError;
  cb->Call(Context::GetCurrent()->Global(), 1, args);
  
  return scope.Close(Undefined());
}

/*
 * GetSQLError
 */

Local<Object> ODBC::GetSQLError (SQLSMALLINT handleType, SQLHANDLE handle) {
  HandleScope scope;
  
  return scope.Close(GetSQLError(
    handleType,
    handle,
    (char *) "[node-odbc] SQL_ERROR"));
}

Local<Object> ODBC::GetSQLError (SQLSMALLINT handleType, SQLHANDLE handle, char* message) {
  HandleScope scope;
  
  DEBUG_PRINTF("ODBC::GetSQLError : handleType=%i, handle=%p\n", handleType, handle);
  
  Local<Object> objError = Object::New();
  Local<String> str = String::New("");

  SQLSMALLINT i = 0;
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

  for (i = 0; i < statusRecCount; i++){
    DEBUG_PRINTF("ODBC::GetSQLError : calling SQLGetDiagRec; i=%i, statusRecCount=%i\n", i, statusRecCount);
    
    ret = SQLGetDiagRec(
      handleType, 
      handle,
      i + 1, 
      (SQLTCHAR *) errorSQLState,
      &native,
      (SQLTCHAR *) errorMessage,
      ERROR_MESSAGE_BUFFER_CHARS,
      &len);
    
    DEBUG_PRINTF("ODBC::GetSQLError : after SQLGetDiagRec; i=%i\n", i);

    if (SQL_SUCCEEDED(ret)) {
      DEBUG_PRINTF("ODBC::GetSQLError : errorMessage=%s, errorSQLState=%s\n", errorMessage, errorSQLState);

      objError->Set(String::New("error"), String::New(message));
#ifdef UNICODE
      str = String::Concat(str, String::New((uint16_t *) errorMessage));

      objError->SetPrototype(Exception::Error(String::New((uint16_t *) errorMessage)));
      objError->Set(String::New("message"), str);
      objError->Set(String::New("state"), String::New((uint16_t *) errorSQLState));
#else
      str = String::Concat(str, String::New(errorMessage));

      objError->SetPrototype(Exception::Error(String::New(errorMessage)));
      objError->Set(String::New("message"), str);
      objError->Set(String::New("state"), String::New(errorSQLState));
#endif
    } else if (ret == SQL_NO_DATA) {
      break;
    }
  }

  if (statusRecCount == 0) {
    //Create a default error object if there were no diag records
    objError->Set(String::New("error"), String::New(message));
    objError->SetPrototype(Exception::Error(String::New(message)));
    objError->Set(String::New("message"), String::New(
      (const char *) "[node-odbc] An error occurred but no diagnostic information was available."));
  }

  return scope.Close(objError);
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
  
  HandleScope scope;
  
  Local<Object> objError = Object::New();
  
  int count = 0;
  int errorCount = 0;
  short colCount = 0;
  
  Column* columns = GetColumns(hSTMT, &colCount);
  
  Local<Array> rows = Array::New();
  
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
      Integer::New(count), 
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
  return scope.Close(rows);
}

#ifdef dynodbc
Handle<Value> ODBC::LoadODBCLibrary(const Arguments& args) {
  HandleScope scope;
  
  REQ_STR_ARG(0, js_library);
  
  bool result = DynLoadODBC(*js_library);
  
  return scope.Close((result) ? True() : False());
}
#endif

extern "C" void init (v8::Handle<Object> target) {
#ifdef dynodbc
  target->Set(String::NewSymbol("loadODBCLibrary"),
        FunctionTemplate::New(ODBC::LoadODBCLibrary)->GetFunction());
#endif
  
  ODBC::Init(target);
  ODBCResult::Init(target);
  ODBCConnection::Init(target);
  ODBCStatement::Init(target);
}

NODE_MODULE(odbc_bindings, init)
