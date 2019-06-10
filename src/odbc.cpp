/*
  Copyright (c) 2019, IBM
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

#include <napi.h>
#include <time.h>
#include <stdlib.h>

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_statement.h"

#ifdef dynodbc
#include "dynodbc.h"
#endif

uv_mutex_t ODBC::g_odbcMutex;
SQLHENV ODBC::hEnv;

Napi::Value ODBC::Init(Napi::Env env, Napi::Object exports) {

  hEnv = NULL;
  Napi::HandleScope scope(env);

  // Wrap ODBC constants in an object that we can then expand
  std::vector<Napi::PropertyDescriptor> ODBC_CONSTANTS;

  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("ODBCVER", Napi::Number::New(env, ODBCVER), napi_enumerable));

  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_COMMIT", Napi::Number::New(env, SQL_COMMIT), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_ROLLBACK", Napi::Number::New(env, SQL_ROLLBACK), napi_enumerable));

  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_USER_NAME", Napi::Number::New(env, SQL_USER_NAME), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_PARAM_INPUT", Napi::Number::New(env, SQL_PARAM_INPUT), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_PARAM_INPUT_OUTPUT", Napi::Number::New(env, SQL_PARAM_INPUT_OUTPUT), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_PARAM_OUTPUT", Napi::Number::New(env, SQL_PARAM_OUTPUT), napi_enumerable));

  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_VARCHAR", Napi::Number::New(env, SQL_VARCHAR), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_INTEGER", Napi::Number::New(env, SQL_INTEGER), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_SMALLINT", Napi::Number::New(env, SQL_SMALLINT), napi_enumerable));

  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_NO_NULLS", Napi::Number::New(env, SQL_NO_NULLS), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_NULLABLE", Napi::Number::New(env, SQL_NULLABLE), napi_enumerable));
  ODBC_CONSTANTS.push_back(Napi::PropertyDescriptor::Value("SQL_NULLABLE_UNKNOWN", Napi::Number::New(env, SQL_NULLABLE_UNKNOWN), napi_enumerable));

  exports.DefineProperties(ODBC_CONSTANTS);

  exports.Set("connect", Napi::Function::New(env, ODBC::Connect));

  // Initialize the cross platform mutex provided by libuv
  uv_mutex_init(&ODBC::g_odbcMutex);

  uv_mutex_lock(&ODBC::g_odbcMutex);
  // Initialize the Environment handle
  SQLRETURN sqlReturnCode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
  uv_mutex_unlock(&ODBC::g_odbcMutex);

  if (!SQL_SUCCEEDED(sqlReturnCode)) {
    DEBUG_PRINTF("ODBC::New - ERROR ALLOCATING ENV HANDLE!!\n");
    Napi::Error(env, Napi::String::New(env, ODBC::GetSQLError(SQL_HANDLE_ENV, hEnv))).ThrowAsJavaScriptException();
    return env.Null();
  }

  // Use ODBC 3.x behavior
  SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, SQL_IS_UINTEGER);

  return exports;
}

ODBC::~ODBC() {
  DEBUG_PRINTF("ODBC::~ODBC\n");

  uv_mutex_lock(&ODBC::g_odbcMutex);

  if (hEnv) {
    SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
    hEnv = NULL;
  }

  uv_mutex_unlock(&ODBC::g_odbcMutex);
}

/*
 * Connect
 */
class ConnectAsyncWorker : public Napi::AsyncWorker {

  public:
    ConnectAsyncWorker(HENV hEnv, SQLTCHAR *connectionStringPtr, unsigned int connectionTimeout, unsigned int loginTimeout, Napi::Function& callback) : Napi::AsyncWorker(callback),
      connectionStringPtr(connectionStringPtr),
      connectionTimeout(connectionTimeout),
      loginTimeout(loginTimeout),
      hEnv(hEnv) {}

    ~ConnectAsyncWorker() {
      delete[] connectionStringPtr;
    }

  private:

    SQLTCHAR *connectionStringPtr;
    unsigned int connectionTimeout;
    unsigned int loginTimeout;
    SQLHENV hEnv;
    SQLHDBC hDBC;

    SQLRETURN sqlReturnCode;

    void Execute() {

      DEBUG_PRINTF("ODBC::ConnectAsyncWorker::Execute\n");

      uv_mutex_lock(&ODBC::g_odbcMutex);

      sqlReturnCode = SQLAllocHandle(
        SQL_HANDLE_DBC,
        hEnv,
        &hDBC);

      if (connectionTimeout > 0) {
        sqlReturnCode = SQLSetConnectAttr(
          hDBC,                                   // ConnectionHandle
          SQL_ATTR_CONNECTION_TIMEOUT,            // Attribute
          (SQLPOINTER) size_t(connectionTimeout), // ValuePtr
          SQL_IS_UINTEGER);                       // StringLength
      }
      
      if (loginTimeout > 0) {
        sqlReturnCode = SQLSetConnectAttr(
          hDBC,                              // ConnectionHandle
          SQL_ATTR_LOGIN_TIMEOUT,            // Attribute
          (SQLPOINTER) size_t(loginTimeout), // ValuePtr
          SQL_IS_UINTEGER);                  // StringLength
      }

      //Attempt to connect
      sqlReturnCode = SQLDriverConnect(
        hDBC,                 // ConnectionHandle
        NULL,                 // WindowHandle
        connectionStringPtr,  // InConnectionString
        SQL_NTS,              // StringLength1
        NULL,                 // OutConnectionString
        0,                    // BufferLength - in characters
        NULL,                 // StringLength2Ptr
        SQL_DRIVER_NOPROMPT); // DriverCompletion

      uv_mutex_unlock(&ODBC::g_odbcMutex);
      ASYNC_WORKER_CHECK_CODE_SET_ERROR_RETURN(sqlReturnCode, SQL_HANDLE_DBC, hDBC, "ConnectAsyncWorker::Execute", "SQLDriverConnect");
    }

    void OnOK() {

      DEBUG_PRINTF("ODBC::ConnectAsyncWorker::OnOk\n");

      Napi::Env env = Env();
      Napi::HandleScope scope(env);

      // pass the HENV and HDBC values to the ODBCConnection constructor
      std::vector<napi_value> connectionArguments;
      connectionArguments.push_back(Napi::External<SQLHENV>::New(env, &hEnv)); // connectionArguments[0]
      connectionArguments.push_back(Napi::External<SQLHDBC>::New(env, &hDBC)); // connectionArguments[1]
        
      Napi::Value connection = ODBCConnection::constructor.New(connectionArguments);

      // pass the arguments to the callback function
      std::vector<napi_value> callbackArguments;
      callbackArguments.push_back(env.Null());  // callbackArguments[0]  
      callbackArguments.push_back(connection); // callbackArguments[1]

      Callback().Call(callbackArguments);
    }
};

// Connect
Napi::Value ODBC::Connect(const Napi::CallbackInfo& info) {

  DEBUG_PRINTF("ODBC::CreateConnection\n");

  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  Napi::String connectionString;
  Napi::Function callback;

  SQLTCHAR *connectionStringPtr = nullptr;
  unsigned int connectionTimeout = 0;
  unsigned int loginTimeout = 0;

  if(info.Length() != 2) {
    Napi::TypeError::New(env, "connect(connectionString, callback) requires 2 parameters.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsString()) {
    connectionString = info[0].As<Napi::String>();
    connectionStringPtr = ODBC::NapiStringToSQLTCHAR(connectionString);
  } else if (info[0].IsObject()) {
    Napi::Object connectionObject = info[0].As<Napi::Object>();
    if (connectionObject.Has("connectionString") && connectionObject.Get("connectionString").IsString()) {
      connectionString = connectionObject.Get("connectionString").As<Napi::String>();
      connectionStringPtr = ODBC::NapiStringToSQLTCHAR(connectionString);
    } else {
      Napi::TypeError::New(env, "connect: A configuration object must have a 'connectionString' property that is a string.").ThrowAsJavaScriptException();
      return env.Null();
    }
    if (connectionObject.Has("connectionTimeout") && connectionObject.Get("connectionTimeout").IsNumber()) {
      connectionTimeout = connectionObject.Get("connectionTimeout").As<Napi::Number>().Int32Value();
    }
    if (connectionObject.Has("loginTimeout") && connectionObject.Get("loginTimeout").IsNumber()) {
      loginTimeout = connectionObject.Get("loginTimeout").As<Napi::Number>().Int32Value();
    }
  } else {
    Napi::TypeError::New(env, "connect: first parameter must be a string or an object.").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[1].IsFunction()) {
    callback = info[1].As<Napi::Function>();
  } else {
    Napi::TypeError::New(env, "connect: second parameter must be a function.").ThrowAsJavaScriptException();
    return env.Null();
  }

  ConnectAsyncWorker *worker = new ConnectAsyncWorker(hEnv, connectionStringPtr, connectionTimeout, loginTimeout, callback);
  worker->Queue();

  return env.Undefined();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////// UTILITY FUNCTIONS ///////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Take a Napi::String, and convert it to an SQLTCHAR*, which maps to:
//    UNICODE : SQLWCHAR*
// no UNICODE : SQLCHAR*
SQLTCHAR* ODBC::NapiStringToSQLTCHAR(Napi::String string) {

  size_t byteCount = 0;

  #ifdef UNICODE
    std::u16string tempString = string.Utf16Value();
    byteCount = (tempString.length() * 2) + 1;
  #else
    std::string tempString = string.Utf8Value();
    byteCount = tempString.length() + 1;
  #endif

  SQLTCHAR *sqlString = new SQLTCHAR[tempString.length() + 1];
  std::memcpy(sqlString, tempString.c_str(), byteCount);
  return sqlString;
}

// Encapsulates the workflow after a result set is returned (many paths require this workflow).
// Does the following:
//   Calls SQLRowCount, which returns the number of rows affected by the statement.
//   Calls ODBC::BindColumns, which calls:
//     SQLNumResultCols to return the number of columns,
//     SQLDescribeCol to describe those columns, and
//     SQLBindCol to bind the column return data to buffers
//   Calls ODBC::FetchAll, which calls:
//     SQLFetch to fetch all of the result rows
//     SQLCloseCursor to close the cursor on the result set
SQLRETURN ODBC::RetrieveResultSet(QueryData *data) {
  SQLRETURN returnCode = SQL_SUCCESS;

  returnCode = SQLRowCount(
    data->hSTMT,    // StatementHandle
    &data->rowCount // RowCountPtr
  );
  if (!SQL_SUCCEEDED(returnCode)) {
    // if SQLRowCount failed, return early with the returnCode
    return returnCode;
  }


  returnCode = ODBC::BindColumns(data);
  if (!SQL_SUCCEEDED(returnCode)) {
    // if BindColumns failed, return early with the returnCode
    return returnCode;
  }

  // data->columnCount is set in ODBC::BindColumns above
  if (data->columnCount > 0) {
    returnCode = ODBC::FetchAll(data);
    // if we got to the end of the result set, thats the happy path
    if (returnCode == SQL_NO_DATA) {
      return SQL_SUCCESS;
    }
  }

  return returnCode;
}

/******************************************************************************
 ****************************** BINDING COLUMNS *******************************
 *****************************************************************************/
SQLRETURN ODBC::BindColumns(QueryData *data) {

  SQLRETURN returnCode = SQL_SUCCESS;

  // SQLNumResultCols returns the number of columns in a result set.
  returnCode = SQLNumResultCols(
    data->hSTMT,       // StatementHandle
    &data->columnCount // ColumnCountPtr
  );

  // if there was an error, set columnCount to 0 and return
  if (!SQL_SUCCEEDED(returnCode)) {
    data->columnCount = 0;
    return returnCode;
  }

  // create Columns for the column data to go into
  data->columns = new Column*[data->columnCount]();
  data->boundRow = new SQLCHAR*[data->columnCount]();

  for (int i = 0; i < data->columnCount; i++) {

    Column *column = new Column();

    column->ColumnName = new SQLTCHAR[SQL_MAX_COLUMN_NAME_LEN]();

    returnCode = SQLDescribeCol(
      data->hSTMT,             // StatementHandle
      i + 1,                   // ColumnNumber
      column->ColumnName,      // ColumnName
      SQL_MAX_COLUMN_NAME_LEN, // BufferLength,
      &column->NameLength,     // NameLengthPtr,
      &column->DataType,       // DataTypePtr
      &column->ColumnSize,     // ColumnSizePtr,
      &column->DecimalDigits,  // DecimalDigitsPtr,
      &column->Nullable        // NullablePtr
    );

    if (!SQL_SUCCEEDED(returnCode)) {
      delete column;
      return returnCode;
    }

    data->columns[i] = column;

    SQLLEN maxColumnLength;
    SQLSMALLINT targetType;

    // bind depending on the column
    switch(column->DataType) {

      case SQL_REAL:
      case SQL_DECIMAL :
      case SQL_NUMERIC :
        maxColumnLength = (column->ColumnSize + 1) * sizeof(SQLCHAR);
        targetType = SQL_C_CHAR;
        break;

      case SQL_DOUBLE :
        maxColumnLength = column->ColumnSize;
        targetType = SQL_C_DOUBLE;
        break;

      case SQL_INTEGER:
      case SQL_SMALLINT:
        maxColumnLength = column->ColumnSize;
        targetType = SQL_C_SLONG;
        break;

      case SQL_BIGINT :
       maxColumnLength = column->ColumnSize;
       targetType = SQL_C_SBIGINT;
       break;

      case SQL_BINARY:
      case SQL_VARBINARY:
      case SQL_LONGVARBINARY:
        maxColumnLength = column->ColumnSize;
        targetType = SQL_C_BINARY;
        break;

      case SQL_WCHAR:
      case SQL_WVARCHAR:
      case SQL_WLONGVARCHAR:
        maxColumnLength = (column->ColumnSize + 1) * sizeof(SQLWCHAR);
        targetType = SQL_C_WCHAR;
        break;

      case SQL_CHAR:
      case SQL_VARCHAR:
      case SQL_LONGVARCHAR:
      default:
        maxColumnLength = (column->ColumnSize + 1) * sizeof(SQLCHAR);
        targetType = SQL_C_CHAR;
        break;
    }

    data->boundRow[i] = new SQLCHAR[maxColumnLength]();

    // SQLBindCol binds application data buffers to columns in the result set.
    returnCode = SQLBindCol(
      data->hSTMT,              // StatementHandle
      i + 1,                    // ColumnNumber
      targetType,               // TargetType
      data->boundRow[i],        // TargetValuePtr
      maxColumnLength,          // BufferLength
      &column->StrLen_or_IndPtr // StrLen_or_Ind
    );

    if (!SQL_SUCCEEDED(data->sqlReturnCode)) {
      return returnCode;
    }
  }

  return returnCode;
}

SQLRETURN ODBC::FetchAll(QueryData *data) {

  // continue call SQLFetch, with results going in the boundRow array
  SQLRETURN returnCode;

  while(SQL_SUCCEEDED(returnCode = SQLFetch(data->hSTMT))) {

    ColumnData *row = new ColumnData[data->columnCount];

    // Iterate over each column, putting the data in the row object
    for (int i = 0; i < data->columnCount; i++) {

      row[i].size = data->columns[i]->StrLen_or_IndPtr;
      if (row[i].size == SQL_NULL_DATA) {
        row[i].data = NULL;
      } else {
        row[i].data = new SQLCHAR[row[i].size];
        memcpy(row[i].data, data->boundRow[i], row[i].size);
      }
    }

    data->storedRows.push_back(row);
  }

  // will return either SQL_ERROR or SQL_NO_DATA
  SQLCloseCursor(data->hSTMT);
  return returnCode;
}

// If we have a parameter with input/output params (e.g. calling a procedure),
// then we need to take the Parameter structures of the QueryData and create
// a Napi::Array from them.
Napi::Array ODBC::ParametersToArray(Napi::Env env, QueryData *data) {
  Parameter **parameters = data->parameters;
  Napi::Array napiParameters = Napi::Array::New(env);

  for (SQLSMALLINT i = 0; i < data->parameterCount; i++) {
    Napi::Value value;

    // check for null data
    if (parameters[i]->StrLen_or_IndPtr == SQL_NULL_DATA) {
      value = env.Null();
    } else {

      switch(parameters[i]->ParameterType) {
        case SQL_REAL:
        case SQL_NUMERIC :
        case SQL_DECIMAL :
          // value = Napi::String::New(env, (const char*)parameters[i]->ParameterValuePtr, parameters[i]->StrLen_or_IndPtr);
          value = Napi::Number::New(env, atof((const char*)parameters[i]->ParameterValuePtr));
          break;
        // Napi::Number
        case SQL_FLOAT :
        case SQL_DOUBLE :
          // value = Napi::String::New(env, (const char*)parameters[i]->ParameterValuePtr);
          value = Napi::Number::New(env, *(double*)parameters[i]->ParameterValuePtr);
          break;
        case SQL_INTEGER :
        case SQL_SMALLINT :
        case SQL_BIGINT :
          value = Napi::Number::New(env, *(int*)parameters[i]->ParameterValuePtr);
          break;
        // Napi::ArrayBuffer
        case SQL_BINARY :
        case SQL_VARBINARY :
        case SQL_LONGVARBINARY :
          value = Napi::ArrayBuffer::New(env, parameters[i]->ParameterValuePtr, parameters[i]->StrLen_or_IndPtr);
          break;
        // Napi::String (char16_t)
        case SQL_WCHAR :
        case SQL_WVARCHAR :
        case SQL_WLONGVARCHAR :
          value = Napi::String::New(env, (const char16_t*)parameters[i]->ParameterValuePtr, parameters[i]->StrLen_or_IndPtr/sizeof(SQLWCHAR));
          break;
        // Napi::String (char)
        case SQL_CHAR :
        case SQL_VARCHAR :
        case SQL_LONGVARCHAR :
        default:
          value = Napi::String::New(env, (const char*)parameters[i]->ParameterValuePtr);
          break;
      }
    }

    napiParameters.Set(i, value);
  }

  return napiParameters;
}

// All of data has been loaded into data->storedRows. Have to take the data
// stored in there and convert it it into JavaScript to be given to the
// Node.js runtime.
Napi::Array ODBC::ProcessDataForNapi(Napi::Env env, QueryData *data) {

  Column **columns = data->columns;
  SQLSMALLINT columnCount = data->columnCount;

  // The rows array is the data structure that is returned from query results.
  // This array holds the records that were returned from the query as objects,
  // with the column names as the property keys on the object and the table
  // values as the property values.
  // Additionally, there are four properties that are added directly onto the
  // array:
  //   'count'   : The  returned from SQLRowCount, which returns "the
  //               number of rows affected by an UPDATE, INSERT, or DELETE
  //               statement." For SELECT statements and other statements
  //               where data is not available, returns -1.
  //   'columns' : An array containing the columns of the result set as
  //               objects, with two properties:
  //                 'name'    : The name of the column
  //                 'dataType': The integer representation of the SQL dataType
  //                             for that column.
  //   'parameters' : An array containing all the parameter values for the
  //                  query. If calling a statement, then parameter values are
  //                  unchanged from the call. If calling a procedure, in/out
  //                  or out parameters may have their values changed.
  //   'return'     : For some procedures, a return code is returned and stored
  //                  in this property.
  //   'statement'  : The SQL statement that was sent to the server. Parameter
  //                  markers are not altered, but parameters passed can be
  //                  determined from the parameters array on this object
  Napi::Array rows = Napi::Array::New(env);

  // set the 'statement' property
  if (data->sql == NULL) {
    rows.Set(Napi::String::New(env, STATEMENT), env.Null());
  } else {
    rows.Set(Napi::String::New(env, STATEMENT), Napi::String::New(env, (const char*)data->sql));
  }

  // set the 'parameters' property
  Napi::Array params = ODBC::ParametersToArray(env, data);
  rows.Set(Napi::String::New(env, PARAMETERS), params);

  // set the 'return' property
  rows.Set(Napi::String::New(env, RETURN), env.Undefined()); // TODO: This doesn't exist on my DBMS of choice, need to test on MSSQL Server or similar

  // set the 'count' property
  rows.Set(Napi::String::New(env, COUNT), Napi::Number::New(env, (double)data->rowCount));

  // construct the array for the 'columns' property and then set
  Napi::Array napiColumns = Napi::Array::New(env);

  for (SQLSMALLINT h = 0; h < columnCount; h++) {
    Napi::Object column = Napi::Object::New(env);
    column.Set(Napi::String::New(env, NAME), Napi::String::New(env, (const char*)columns[h]->ColumnName));
    column.Set(Napi::String::New(env, DATA_TYPE), Napi::Number::New(env, columns[h]->DataType));
    napiColumns.Set(h, column);
  }
  rows.Set(Napi::String::New(env, COLUMNS), napiColumns);

  // iterate over all of the stored rows,
  for (size_t i = 0; i < data->storedRows.size(); i++) {

    Napi::Object row = Napi::Object::New(env);

    ColumnData *storedRow = data->storedRows[i];

    // Iterate over each column, putting the data in the row object
    for (SQLSMALLINT j = 0; j < columnCount; j++) {

      Napi::Value value;

      // check for null data
      if (storedRow[j].size == SQL_NULL_DATA) {

        value = env.Null();

      } else {

        switch(columns[j]->DataType) {
          case SQL_REAL:
          case SQL_DECIMAL:
          case SQL_NUMERIC:
            value = Napi::Number::New(env, atof((const char*)storedRow[j].data));
            break;
          // Napi::Number
          case SQL_FLOAT:
          case SQL_DOUBLE:
            value = Napi::Number::New(env, *(double*)storedRow[j].data);
            break;
          case SQL_INTEGER:
          case SQL_SMALLINT:
          case SQL_BIGINT:
            value = Napi::Number::New(env, *(int*)storedRow[j].data);
            break;
          // Napi::ArrayBuffer
          case SQL_BINARY :
          case SQL_VARBINARY :
          case SQL_LONGVARBINARY :
            value = Napi::ArrayBuffer::New(env, storedRow[j].data, storedRow[j].size);
            break;
          // Napi::String (char16_t)
          case SQL_WCHAR :
          case SQL_WVARCHAR :
          case SQL_WLONGVARCHAR :
            value = Napi::String::New(env, (const char16_t*)storedRow[j].data, storedRow[j].size/sizeof(SQLWCHAR));
            break;
          // Napi::String (char)
          case SQL_CHAR :
          case SQL_VARCHAR :
          case SQL_LONGVARCHAR :
          default:
            value = Napi::String::New(env, (const char*)storedRow[j].data, storedRow[j].size);
            break;
        }
      }
      row.Set(Napi::String::New(env, (const char*)columns[j]->ColumnName), value);
    }
    rows.Set(i, row);
  }

  return rows;
}

/******************************************************************************
 **************************** BINDING PARAMETERS ******************************
 *****************************************************************************/

/*
 * GetParametersFromArray
 *  Array of parameters can hold either/and:
 *    Value:
 *      One value to bind, In/Out defaults to SQL_PARAM_INPUT, dataType defaults based on the value
 *    Arrays:ns when you elect n
 *      between 1 and 3 entries in lenth, with the following signfigance and default values:
 *        1. Value (REQUIRED): The value to bind
 *        2. In/Out (Optional): Defaults to SQL_PARAM_INPUT
 *        3. DataType (Optional): Defaults based on the value 
 *    Objects:
 *      can hold any of the following properties (but requires at least 'value' property)
 *        value (Requited): The value to bind
 *        inOut (Optional): the In/Out type to use, Defaults to SQL_PARAM_INPUT
 *        dataType (Optional): The data type, defaults based on the value
 *
 *
 */


// This function solves the problem of losing access to "Napi::Value"s when entering
// an AsyncWorker. In Connection::Query, once we enter an AsyncWorker we do not leave it again,
// but we can't call SQLNumParams and SQLDescribeParam until after SQLPrepare. So the array of
// values to bind to parameters must be saved off in the closest, largest data type to then
// convert to the right C Type once the SQL Type of the parameter is known.
void ODBC::StoreBindValues(Napi::Array *values, Parameter **parameters) {

  uint32_t numParameters = values->Length();

  for (uint32_t i = 0; i < numParameters; i++) {

    Napi::Value value = values->Get(i);
    Parameter *parameter = parameters[i];

    if(value.IsNull()) {
      parameter->ValueType = SQL_C_DEFAULT;
      parameter->ParameterValuePtr = NULL;
      parameter->StrLen_or_IndPtr = SQL_NULL_DATA;
    } else if (value.IsNumber()) {
      double double_val = value.As<Napi::Number>().DoubleValue();
      int64_t int_val = value.As<Napi::Number>().Int64Value();
      if ((int64_t)double_val == int_val) {
        parameter->ValueType = SQL_C_SBIGINT;
        parameter->ParameterValuePtr = new int64_t(value.As<Napi::Number>().Int64Value());
      } else {
        parameter->ValueType = SQL_C_DOUBLE;
        parameter->ParameterValuePtr = new double(value.As<Napi::Number>().DoubleValue());
      }
    } else if (value.IsBoolean()) {
      parameter->ValueType = SQL_C_BIT;
      parameter->ParameterValuePtr = new bool(value.As<Napi::Boolean>().Value());
    } else if (value.IsString()) {
      Napi::String string = value.ToString();
      parameter->ValueType = SQL_C_TCHAR;
      parameter->BufferLength = (string.Utf8Value().length() + 1) * sizeof(SQLTCHAR);
      parameter->ParameterValuePtr = new SQLTCHAR[parameter->BufferLength];
      memcpy((SQLTCHAR*) parameter->ParameterValuePtr, string.Utf8Value().c_str(), parameter->BufferLength);
      parameter->StrLen_or_IndPtr = SQL_NTS;
    } else {
      // TODO: Throw error, don't support other types
    }
  }
}

SQLRETURN ODBC::DescribeParameters(SQLHSTMT hSTMT, Parameter **parameters, SQLSMALLINT parameterCount) {

  SQLRETURN returnCode = SQL_SUCCESS; // if no parameters, will return SQL_SUCCESS

  for (SQLSMALLINT i = 0; i < parameterCount; i++) {

    Parameter *parameter = parameters[i];

    // "Except in calls to procedures, all parameters in SQL statements are input parameters."
    parameter->InputOutputType = SQL_PARAM_INPUT;

    returnCode = SQLDescribeParam(
      hSTMT,                     // StatementHandle,
      i + 1,                     // ParameterNumber,
      &parameter->ParameterType, // DataTypePtr,
      &parameter->ColumnSize,    // ParameterSizePtr,
      &parameter->DecimalDigits, // DecimalDigitsPtr,
      NULL                       // NullablePtr // Don't care for this package, send NULLs and get error
    );

    // if there is an error, return early and retrieve error in calling function
    if (!SQL_SUCCEEDED(returnCode)) {
      return returnCode;
    }
  }

  return returnCode;
}

SQLRETURN ODBC::BindParameters(SQLHSTMT hSTMT, Parameter **parameters, SQLSMALLINT parameterCount) {

  SQLRETURN sqlReturnCode = SQL_SUCCESS; // if no parameters, will return SQL_SUCCESS

  for (int i = 0; i < parameterCount; i++) {

    Parameter* parameter = parameters[i];

    sqlReturnCode = SQLBindParameter(
      hSTMT,                        // StatementHandle
      i + 1,                        // ParameterNumber
      parameter->InputOutputType,   // InputOutputType
      parameter->ValueType,         // ValueType
      parameter->ParameterType,     // ParameterType
      parameter->ColumnSize,        // ColumnSize
      parameter->DecimalDigits,     // DecimalDigits
      parameter->ParameterValuePtr, // ParameterValuePtr
      parameter->BufferLength,      // BufferLength
      &parameter->StrLen_or_IndPtr  // StrLen_or_IndPtr
    );

    // If there was an error, return early
    if (!SQL_SUCCEEDED(sqlReturnCode)) {
      return sqlReturnCode;
    }
  }

  // If returns success, know that SQLBindParameter returned SUCCESS or
  // SUCCESS_WITH_INFO for all calls to SQLBindParameter.
  return sqlReturnCode;
}

/******************************************************************************
 ********************************** ERRORS ************************************
 *****************************************************************************/

/*
 * GetSQLError
 */

std::string ODBC::GetSQLError(SQLSMALLINT handleType, SQLHANDLE handle) {

  std::string error = GetSQLError(handleType, handle, "[node-odbc] SQL_ERROR");
  return error;
}

std::string ODBC::GetSQLError(SQLSMALLINT handleType, SQLHANDLE handle, const char* message) {

  DEBUG_PRINTF("ODBC::GetSQLError : handleType=%i, handle=%p\n", handleType, handle);

  std::string errorMessageStr;

  // Napi::Object objError = Napi::Object::New(env);

  int32_t i = 0;
  SQLINTEGER native;

  SQLSMALLINT len;
  SQLINTEGER statusRecCount = 0;
  SQLRETURN ret;
  char errorSQLState[14];
  char errorMessage[ERROR_MESSAGE_BUFFER_BYTES];

  ret = SQLGetDiagField (
    handleType,
    handle,
    0,
    SQL_DIAG_NUMBER,
    &statusRecCount,
    SQL_IS_INTEGER,
    &len
  );

  // Windows seems to define SQLINTEGER as long int, unixodbc as just int... %i should cover both
  DEBUG_PRINTF("ODBC::GetSQLError : called SQLGetDiagField; ret=%i, statusRecCount=%i\n", ret, statusRecCount);

  // Napi::Array errors = Napi::Array::New(env);

  if (statusRecCount > 1) {
    // objError.Set(Napi::String::New(env, "errors"), errors);
  }

  errorMessageStr += "\"errors\": [";

  for (i = 0; i < statusRecCount; i++) {

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


      if (i != 0) {
        errorMessageStr += ',';
      }

#ifdef UNICODE
// TODO:
        std::string error = message;
        std::string message = errorMessage;
        std::string SQLstate = errorSQLState;
#else
        std::string error = message;
        std::string message = errorMessage;
        std::string SQLstate = errorSQLState;
#endif
        errorMessageStr += "{ \"error\": \"" + error + "\", \"message\": \"" + errorMessage + "\", \"SQLState\": \"" + SQLstate + "\"}";

    } else if (ret == SQL_NO_DATA) {
      break;
    }
  }

  // if (statusRecCount == 0) {
  //   //Create a default error object if there were no diag records
  //   objError.Set(Napi::String::New(env, "error"), Napi::String::New(env, message));
  //   //objError.SetPrototype(Napi::Error(Napi::String::New(env, message)));
  //   objError.Set(Napi::String::New(env, "message"), Napi::String::New(env, 
  //     (const char *) "[node-odbc] An error occurred but no diagnostic information was available."));
  // }

  errorMessageStr += "]";

  return errorMessageStr;
}

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {

  ODBC::Init(env, exports);
  ODBCConnection::Init(env, exports);
  ODBCStatement::Init(env, exports);

  #ifdef dynodbc
    exports.Set(Napi::String::New(env, "loadODBCLibrary"),
                Napi::Function::New(env, ODBC::LoadODBCLibrary);());
  #endif

  return exports;
}

#ifdef dynodbc
Napi::Value ODBC::LoadODBCLibrary(const Napi::CallbackInfo& info) {
  Napi::HandleScope scope(env);

  REQ_STR_ARG(0, js_library);

  bool result = DynLoadODBC(*js_library);

  return (result) ? env.True() : env.False();W
}
#endif

NODE_API_MODULE(odbc_bindings, InitAll)
