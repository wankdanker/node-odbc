/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');

describe('.query(sql, [parameters], [callback])...', () => {
  it('...should throw a TypeError if function signature doesn\'t match accepted signatures.', async () => {
    const QUERY_TYPE_ERROR = {
      name: 'TypeError',
      message: '[node-odbc]: Incorrect function signature for call to connection.query({string}, {array}[optional], {function}[optional]).',
    };
    const QUERY_CALLBACK = () => {};
    const QUERY_STRING = `SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`;

    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    assert.throws(() => {
      connection.query();
    }, QUERY_TYPE_ERROR);
    assert.throws(() => {
      connection.query(QUERY_CALLBACK);
    }, QUERY_TYPE_ERROR);
    assert.throws(() => {
      connection.query(1, []);
    }, QUERY_TYPE_ERROR);
    assert.throws(() => {
      connection.query(1, [], QUERY_CALLBACK);
    }, QUERY_TYPE_ERROR);
    assert.throws(() => {
      connection.query(QUERY_STRING, {});
    }, QUERY_TYPE_ERROR);
    assert.throws(() => {
      connection.query(QUERY_STRING, {}, QUERY_CALLBACK);
    }, QUERY_TYPE_ERROR);
    assert.throws(() => {
      connection.query(1, 1);
    }, QUERY_TYPE_ERROR);
    assert.throws(() => {
      connection.query(1, 1, QUERY_CALLBACK);
    }, QUERY_TYPE_ERROR);
    await connection.close();
  });
  describe('...with callbacks...', () => {
    it('...should correctly identify function signature with .query({string}, {function}).', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`, (error1, result1) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error2, result2) => {
            assert.deepEqual(error2, null);
            assert.notDeepEqual(result2, null);
            assert.deepEqual(result2.length, 1);
            assert.deepEqual(result2.count, -1);
            assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
            connection.close((error3) => {
              assert.deepEqual(error3, null);
              done();
            });
          });
        });
      });
    });
    it('...should correctly identify function signature with .query({string}, {array}, {function})', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10], (error1, result1) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.length, 0);
          assert.deepEqual(result1.count, 1);
          connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error2, result2) => {
            assert.deepEqual(error2, null);
            assert.notDeepEqual(result2, null);
            assert.deepEqual(result2.length, 1);
            assert.deepEqual(result2.count, -1);
            assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
            connection.close((error3) => {
              assert.deepEqual(error3, null);
              done();
            });
          });
        });
      });
    });
  }); // ...with callbacks...
  describe('...with promises...', () => {
    it('...should correctly identify function signature with .query({string}).', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2.count, -1);
      assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
      await connection.close();
    });
    it('...should correctly identify function signature with .query({string}, {array})', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, [1, 'committed', 10]);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.length, 0);
      assert.deepEqual(result1.count, 1);
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2.count, -1);
      assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
      await connection.close();
    });
  }); // ...with promises...
}); // '.query(sql, [parameters], [callback])...'
