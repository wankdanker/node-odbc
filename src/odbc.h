/*
  Copyright (c) 2019, IBM
  Copyright (c) 2013, Dan VerWeire <dverweire@gmail.com>
  Copyright (c) 2010, Lee Smith <notwink@gmail.com>

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

#ifndef _SRC_ODBC_H
#define _SRC_ODBC_H

#include <uv.h>
#include <napi.h>
#include <wchar.h>

#include <stdlib.h>
#ifdef dynodbc
#include "dynodbc.h"
#else
#include <sql.h>
#include <sqltypes.h>
#include <sqlext.h>
#include <sqlucode.h>
#endif

#define MAX_FIELD_SIZE 1024
#define MAX_VALUE_SIZE 1048576

#ifdef UNICODE
#define ERROR_MESSAGE_BUFFER_BYTES 2048
#define ERROR_MESSAGE_BUFFER_CHARS 1024
#else
#define ERROR_MESSAGE_BUFFER_BYTES 2048
#define ERROR_MESSAGE_BUFFER_CHARS 2048
#endif

#define FETCH_ARRAY 3
#define FETCH_OBJECT 4
#define SQL_DESTROY 9999

// object keys for the result object
static const std::string NAME = "name";
static const std::string DATA_TYPE = "dataType";
static const std::string STATEMENT = "statement";
static const std::string PARAMETERS = "parameters";
static const std::string RETURN = "return";
static const std::string COUNT = "count";
static const std::string COLUMNS = "columns";

typedef struct Column {
  SQLUSMALLINT  index;
  SQLTCHAR      *ColumnName = NULL;
  SQLSMALLINT   BufferLength;
  SQLSMALLINT   NameLength;
  SQLSMALLINT   DataType;
  SQLULEN       ColumnSize;
  SQLSMALLINT   DecimalDigits;
  SQLLEN        StrLen_or_IndPtr;
  SQLSMALLINT   Nullable;
} Column;

// Amalgamation of the information returned by SQLDescribeParam/SQLProcedureColumns and the
// information needed by SQLBindParameter
typedef struct Parameter {
  SQLSMALLINT InputOutputType; // not returned by SQLDescribeParam, but is by SQLProcedureColumns
  SQLSMALLINT ValueType;
  SQLSMALLINT ParameterType;
  SQLULEN     ColumnSize;
  SQLSMALLINT DecimalDigits;
  SQLPOINTER  ParameterValuePtr;
  SQLLEN      BufferLength;
  SQLLEN      StrLen_or_IndPtr;
  SQLSMALLINT Nullable;
} Parameter;

typedef struct ColumnData {
  SQLCHAR  *data;
  SQLLEN    size;

  ~ColumnData() {
    delete[] this->data;
  }

} ColumnData;

// QueryData
typedef struct QueryData {

  SQLHSTMT hSTMT;

  // parameters
  SQLSMALLINT parameterCount = 0; // returned by SQLNumParams
  SQLSMALLINT bindValueCount = 0; // number of values passed from JavaScript
  Parameter** parameters = NULL;

  // columns and rows
  Column                   **columns = NULL;
  SQLSMALLINT                columnCount;
  SQLCHAR                  **boundRow = NULL;
  std::vector<ColumnData*>   storedRows;
  SQLLEN                     rowCount;

  // query options
  SQLTCHAR *sql       = NULL;
  SQLTCHAR *catalog   = NULL;
  SQLTCHAR *schema    = NULL;
  SQLTCHAR *table     = NULL;
  SQLTCHAR *type      = NULL;
  SQLTCHAR *column    = NULL;
  SQLTCHAR *procedure = NULL;

  SQLRETURN sqlReturnCode;

  ~QueryData() {
    this->clear();
  }

  void deleteColumns() {
    if (this->columnCount > 0) {
      for (int i = 0; i < this->columnCount; i++) {
        delete this->columns[i]->ColumnName;
        delete this->columns[i];
      }
    }

    delete columns; columns = NULL;
    delete boundRow; boundRow = NULL;
    delete sql; sql = NULL;
  }

  void clear() {

    for (size_t h = 0; h < this->storedRows.size(); h++) {
      delete[] storedRows[h];
    };

    int numParameters = std::max(this->bindValueCount, this->parameterCount);

    if (numParameters > 0) {

      Parameter* parameter;

      for (int i = 0; i < numParameters; i++) {
        if (parameter = this->parameters[i], parameter->ParameterValuePtr != NULL) {
          switch (parameter->ValueType) {
            case SQL_C_SBIGINT:
              delete (int64_t*)parameter->ParameterValuePtr;
              break;
            case SQL_C_DOUBLE:
              delete (double*)parameter->ParameterValuePtr;
              break;
            case SQL_C_BIT:
              delete (bool*)parameter->ParameterValuePtr;
              break;
            case SQL_C_TCHAR:
            default:
              delete[] (SQLTCHAR*)parameter->ParameterValuePtr;
              break;
          }
        }
        parameter->ParameterValuePtr = NULL;
        delete parameter;
      }

      delete[] this->parameters; this->parameters = NULL;
      this->bindValueCount = 0;
      this->parameterCount = 0;
    }

    if (this->columnCount > 0) {
      for (int i = 0; i < this->columnCount; i++) {
        delete[] this->boundRow[i];
        delete[] this->columns[i]->ColumnName;
        delete this->columns[i];
      }
    }

    delete[] columns; columns = NULL;
    delete[] boundRow; boundRow = NULL;

    delete[] this->sql; this->sql = NULL;
    delete[] this->catalog; this->catalog = NULL;
    delete[] this->schema; this->schema = NULL;
    delete[] this->table; this->table = NULL;
    delete[] this->type; this->type = NULL;
    delete[] this->column; this->column = NULL;
  }

} QueryData;

class ODBC {

  public:
    static uv_mutex_t g_odbcMutex;
    static SQLHENV hEnv;

    static Napi::Value Init(Napi::Env env, Napi::Object exports);

    static std::string GetSQLError(SQLSMALLINT handleType, SQLHANDLE handle);
    static std::string GetSQLError(SQLSMALLINT handleType, SQLHANDLE handle, const char* message);

    static SQLTCHAR* NapiStringToSQLTCHAR(Napi::String string);

    static SQLRETURN RetrieveResultSet(QueryData *data);
    static SQLRETURN BindColumns(QueryData *data);
    static SQLRETURN FetchAll(QueryData *data);

    static void StoreBindValues(Napi::Array *values, Parameter **parameters);
    static SQLRETURN DescribeParameters(SQLHSTMT hSTMT, Parameter **parameters, SQLSMALLINT parameterCount);
    static SQLRETURN  BindParameters(SQLHSTMT hSTMT, Parameter **parameters, SQLSMALLINT parameterCount);
    static Napi::Array ParametersToArray(Napi::Env env, QueryData *data);

    static Napi::Array ProcessDataForNapi(Napi::Env env, QueryData *data);

    void Free();

    ~ODBC();

    static Napi::Value Connect(const Napi::CallbackInfo& info);

    #ifdef dynodbc
    static Napi::Value LoadODBCLibrary(const Napi::CallbackInfo& info);
    #endif
};

#ifdef DEBUG
#define DEBUG_TPRINTF(...) fprintf(stdout, __VA_ARGS__)
#define DEBUG_PRINTF(...) fprintf(stdout, __VA_ARGS__)
#else
#define DEBUG_PRINTF(...) (void)0
#define DEBUG_TPRINTF(...) (void)0
#endif


#define ASYNC_WORKER_CHECK_CODE_SET_ERROR_RETURN(returnCode, handletype, handle, context, sqlFunction) \
{\
  if(!SQL_SUCCEEDED(returnCode)) {\
    char errorString[255];\
    sprintf(errorString, "[Node.js::odbc] %s: Error in ODBC function %s", context, sqlFunction);\
    SetError(ODBC::GetSQLError(handletype, handle, errorString));\
    return;\
  }\
}

#endif