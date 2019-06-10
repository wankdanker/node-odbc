/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const nativeOdbc = require('../../build/Release/odbc.node');
const odbc = require('../../');

describe('.tables(catalog, schema, table, type, callback)...', () => {
  describe('...with callbacks...', () => {
    it('...should return information about a table.', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.tables(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_TABLE}`, null, (error1, results) => {
          assert.strictEqual(error1, null);
          assert.strictEqual(results.length, 1);
          assert.strictEqual(results.count, 1);
          assert.deepStrictEqual(results.columns,
            [
              { name: 'TABLE_CAT', dataType: nativeOdbc.SQL_VARCHAR },
              { name: 'TABLE_SCHEM', dataType: nativeOdbc.SQL_VARCHAR },
              { name: 'TABLE_NAME', dataType: nativeOdbc.SQL_VARCHAR },
              { name: 'TABLE_TYPE', dataType: nativeOdbc.SQL_VARCHAR },
              { name: 'REMARKS', dataType: nativeOdbc.SQL_VARCHAR },
            ]);
          const result = results[0];
          // not testing for TABLE_CAT, dependent on the system
          assert.strictEqual(result.TABLE_SCHEM, `${process.env.DB_SCHEMA}`);
          assert.strictEqual(result.TABLE_NAME, `${process.env.DB_TABLE}`);
          assert.strictEqual(result.TABLE_TYPE, 'TABLE');
          // not testing for REMARKS, dependent on the system
          done();
        });
      });
    });
    it('...should return empty with bad parameters.', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.tables(null, 'bad schema name', 'bad table name', null, (error1, results) => {
          assert.strictEqual(error1, null);
          assert.strictEqual(results.length, 0);
          assert.strictEqual(results.count, 0);
          assert.deepStrictEqual(results.columns,
            [
              { name: 'TABLE_CAT', dataType: nativeOdbc.SQL_VARCHAR },
              { name: 'TABLE_SCHEM', dataType: nativeOdbc.SQL_VARCHAR },
              { name: 'TABLE_NAME', dataType: nativeOdbc.SQL_VARCHAR },
              { name: 'TABLE_TYPE', dataType: nativeOdbc.SQL_VARCHAR },
              { name: 'REMARKS', dataType: nativeOdbc.SQL_VARCHAR },
            ]);
          done();
        });
      });
    });
  }); // ...with callbacks...
  describe('...with promises...', () => {
    it('...should return information about a table.', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const results = await connection.tables(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_TABLE}`, null);
      assert.strictEqual(results.length, 1);
      assert.strictEqual(results.count, 1);
      assert.deepStrictEqual(results.columns,
        [
          { name: 'TABLE_CAT', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'TABLE_SCHEM', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'TABLE_NAME', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'TABLE_TYPE', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'REMARKS', dataType: nativeOdbc.SQL_VARCHAR },
        ]);
      const result = results[0];
      // not testing for TABLE_CAT, dependent on the system
      assert.strictEqual(result.TABLE_SCHEM, `${process.env.DB_SCHEMA}`);
      assert.strictEqual(result.TABLE_NAME, `${process.env.DB_TABLE}`);
      assert.strictEqual(result.TABLE_TYPE, 'TABLE');
      // not testing for REMARKS, dependent on the system
    });
    it('...should return empty with bad parameters.', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const results = await connection.tables(null, 'bad schema name', 'bad table name', null);
      assert.strictEqual(results.length, 0);
      assert.strictEqual(results.count, 0);
      assert.deepStrictEqual(results.columns,
        [
          { name: 'TABLE_CAT', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'TABLE_SCHEM', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'TABLE_NAME', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'TABLE_TYPE', dataType: nativeOdbc.SQL_VARCHAR },
          { name: 'REMARKS', dataType: nativeOdbc.SQL_VARCHAR },
        ]);
    });
  }); // ...with promises...
});
