/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');
const nativeOdbc = require('../../build/Release/odbc.node');

describe('.columns(catalog, schema, table, column, callback)...', () => {
  let connection = null;
  beforeEach(async () => {
    connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
  });

  afterEach(async () => {
    await connection.close();
    connection = null;
  });
  describe('...with callbacks...', () => {
    it('...should return information about all columns of a table.', (done) => {
      connection.columns(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_TABLE}`, null, (error, results) => {
        assert.strictEqual(error, null);
        assert.strictEqual(results.length, 3);
        assert.strictEqual(results.count, 3);
        assert.deepStrictEqual(results.columns,
          [
            { name: 'TABLE_CAT', dataType: nativeOdbc.SQL_VARCHAR },
            { name: 'TABLE_SCHEM', dataType: nativeOdbc.SQL_VARCHAR },
            { name: 'TABLE_NAME', dataType: nativeOdbc.SQL_VARCHAR },
            { name: 'COLUMN_NAME', dataType: nativeOdbc.SQL_VARCHAR },
            { name: 'DATA_TYPE', dataType: nativeOdbc.SQL_SMALLINT },
            { name: 'TYPE_NAME', dataType: nativeOdbc.SQL_VARCHAR },
            { name: 'COLUMN_SIZE', dataType: nativeOdbc.SQL_INTEGER },
            { name: 'BUFFER_LENGTH', dataType: nativeOdbc.SQL_INTEGER },
            { name: 'DECIMAL_DIGITS', dataType: nativeOdbc.SQL_SMALLINT },
            { name: 'NUM_PREC_RADIX', dataType: nativeOdbc.SQL_SMALLINT },
            { name: 'NULLABLE', dataType: nativeOdbc.SQL_SMALLINT },
            { name: 'REMARKS', dataType: nativeOdbc.SQL_VARCHAR },
            { name: 'COLUMN_DEF', dataType: nativeOdbc.SQL_VARCHAR },
            { name: 'SQL_DATA_TYPE', dataType: nativeOdbc.SQL_SMALLINT },
            { name: 'SQL_DATETIME_SUB', dataType: nativeOdbc.SQL_SMALLINT },
            { name: 'CHAR_OCTET_LENGTH', dataType: nativeOdbc.SQL_INTEGER },
            { name: 'ORDINAL_POSITION', dataType: nativeOdbc.SQL_INTEGER },
            { name: 'IS_NULLABLE', dataType: nativeOdbc.SQL_VARCHAR },
          ]);
        const idColumn = results[0];
        assert.deepEqual(idColumn.COLUMN_NAME, 'ID');
        assert.deepEqual(idColumn.DATA_TYPE, nativeOdbc.SQL_INTEGER);
        assert.deepEqual(idColumn.NULLABLE, nativeOdbc.SQL_NULLABLE);

        const nameColumn = results[1];
        assert.deepEqual(nameColumn.COLUMN_NAME, 'NAME');
        assert.deepEqual(nameColumn.DATA_TYPE, nativeOdbc.SQL_VARCHAR);
        assert.deepEqual(nameColumn.NULLABLE, nativeOdbc.SQL_NULLABLE);

        const ageColumn = results[2];
        assert.deepEqual(ageColumn.COLUMN_NAME, 'AGE');
        assert.deepEqual(ageColumn.DATA_TYPE, nativeOdbc.SQL_INTEGER);
        assert.deepEqual(ageColumn.NULLABLE, nativeOdbc.SQL_NULLABLE);
        done();
      });
    });
    it('...should return empty with bad parameters.', (done) => {
      connection.columns(null, 'bad schema name', 'bad table name', null, (error, results) => {
        assert.strictEqual(error, null);
        assert.strictEqual(results.length, 0);
        assert.strictEqual(results.count, 0);
        assert.deepStrictEqual(results.columns,
          [
            { name: 'TABLE_CAT', dataType: nativeOdbc.SQL_VARCHAR },
            { name: 'TABLE_SCHEM', dataType: nativeOdbc.SQL_VARCHAR },
            { name: 'TABLE_NAME', dataType: nativeOdbc.SQL_VARCHAR },
            { name: 'COLUMN_NAME', dataType: nativeOdbc.SQL_VARCHAR },
            { name: 'DATA_TYPE', dataType: nativeOdbc.SQL_SMALLINT },
            { name: 'TYPE_NAME', dataType: nativeOdbc.SQL_VARCHAR },
            { name: 'COLUMN_SIZE', dataType: nativeOdbc.SQL_INTEGER },
            { name: 'BUFFER_LENGTH', dataType: nativeOdbc.SQL_INTEGER },
            { name: 'DECIMAL_DIGITS', dataType: nativeOdbc.SQL_SMALLINT },
            { name: 'NUM_PREC_RADIX', dataType: nativeOdbc.SQL_SMALLINT },
            { name: 'NULLABLE', dataType: nativeOdbc.SQL_SMALLINT },
            { name: 'REMARKS', dataType: nativeOdbc.SQL_VARCHAR },
            { name: 'COLUMN_DEF', dataType: nativeOdbc.SQL_VARCHAR },
            { name: 'SQL_DATA_TYPE', dataType: nativeOdbc.SQL_SMALLINT },
            { name: 'SQL_DATETIME_SUB', dataType: nativeOdbc.SQL_SMALLINT },
            { name: 'CHAR_OCTET_LENGTH', dataType: nativeOdbc.SQL_INTEGER },
            { name: 'ORDINAL_POSITION', dataType: nativeOdbc.SQL_INTEGER },
            { name: 'IS_NULLABLE', dataType: nativeOdbc.SQL_VARCHAR },
          ]);
        done();
      });
    });
  }); // ...with callbacks...
  describe('...with promises...', () => {
    it('...should return information about all columns of a table.', async () => {
      const results = await connection.columns(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_TABLE}`, null);
      assert.strictEqual(results.length, 3);
      assert.strictEqual(results.count, 3);
      assert.deepStrictEqual(results.columns,
        [
          { name: 'TABLE_CAT', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'TABLE_SCHEM', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'TABLE_NAME', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'COLUMN_NAME', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'DATA_TYPE', dataType: nativeOdbc.SQL_SMALLINT },
          { name: 'TYPE_NAME', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'COLUMN_SIZE', dataType: nativeOdbc.SQL_INTEGER },
          { name: 'BUFFER_LENGTH', dataType: nativeOdbc.SQL_INTEGER },
          { name: 'DECIMAL_DIGITS', dataType: nativeOdbc.SQL_SMALLINT },
          { name: 'NUM_PREC_RADIX', dataType: nativeOdbc.SQL_SMALLINT },
          { name: 'NULLABLE', dataType: nativeOdbc.SQL_SMALLINT },
          { name: 'REMARKS', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'COLUMN_DEF', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'SQL_DATA_TYPE', dataType: nativeOdbc.SQL_SMALLINT },
          { name: 'SQL_DATETIME_SUB', dataType: nativeOdbc.SQL_SMALLINT },
          { name: 'CHAR_OCTET_LENGTH', dataType: nativeOdbc.SQL_INTEGER },
          { name: 'ORDINAL_POSITION', dataType: nativeOdbc.SQL_INTEGER },
          { name: 'IS_NULLABLE', dataType: nativeOdbc.SQL_VARCHAR },
        ]);
      const idColumn = results[0];
      assert.deepEqual(idColumn.COLUMN_NAME, 'ID');
      assert.deepEqual(idColumn.DATA_TYPE, nativeOdbc.SQL_INTEGER);
      assert.deepEqual(idColumn.NULLABLE, nativeOdbc.SQL_NULLABLE);

      const nameColumn = results[1];
      assert.deepEqual(nameColumn.COLUMN_NAME, 'NAME');
      assert.deepEqual(nameColumn.DATA_TYPE, nativeOdbc.SQL_VARCHAR);
      assert.deepEqual(nameColumn.NULLABLE, nativeOdbc.SQL_NULLABLE);

      const ageColumn = results[2];
      assert.deepEqual(ageColumn.COLUMN_NAME, 'AGE');
      assert.deepEqual(ageColumn.DATA_TYPE, nativeOdbc.SQL_INTEGER);
      assert.deepEqual(ageColumn.NULLABLE, nativeOdbc.SQL_NULLABLE);
    });
    it('...should return empty with bad parameters.', async () => {
      const results = await connection.columns(null, 'bad schema name', 'bad table name', null);
      assert.strictEqual(results.length, 0);
      assert.strictEqual(results.count, 0);
      assert.deepStrictEqual(results.columns,
        [
          { name: 'TABLE_CAT', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'TABLE_SCHEM', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'TABLE_NAME', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'COLUMN_NAME', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'DATA_TYPE', dataType: nativeOdbc.SQL_SMALLINT },
          { name: 'TYPE_NAME', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'COLUMN_SIZE', dataType: nativeOdbc.SQL_INTEGER },
          { name: 'BUFFER_LENGTH', dataType: nativeOdbc.SQL_INTEGER },
          { name: 'DECIMAL_DIGITS', dataType: nativeOdbc.SQL_SMALLINT },
          { name: 'NUM_PREC_RADIX', dataType: nativeOdbc.SQL_SMALLINT },
          { name: 'NULLABLE', dataType: nativeOdbc.SQL_SMALLINT },
          { name: 'REMARKS', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'COLUMN_DEF', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'SQL_DATA_TYPE', dataType: nativeOdbc.SQL_SMALLINT },
          { name: 'SQL_DATETIME_SUB', dataType: nativeOdbc.SQL_SMALLINT },
          { name: 'CHAR_OCTET_LENGTH', dataType: nativeOdbc.SQL_INTEGER },
          { name: 'ORDINAL_POSITION', dataType: nativeOdbc.SQL_INTEGER },
          { name: 'IS_NULLABLE', dataType: nativeOdbc.SQL_VARCHAR },
        ]);
    });
  }); // ...with promises...
});
