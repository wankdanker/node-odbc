/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');

describe('.rollback(callback)...', () => {
  describe('...with callbacks...', () => {
    it('...should rollback all queries from after beginTransaction() was called.', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.beginTransaction((error1) => {
          assert.deepEqual(error1, null);
          connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(2, 'rolledback', 20)`, (error2, result2) => {
            assert.deepEqual(error2, null);
            assert.notDeepEqual(result2, null);
            assert.deepEqual(result2.count, 1);
            connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error3, result3) => {
              assert.deepEqual(error3, null);
              assert.deepEqual(result3.length, 1);
              assert.deepEqual(result3.count, -1);
              assert.deepEqual(result3[0], { ID: 2, NAME: 'rolledback', AGE: 20 });
              connection.rollback((error4) => {
                assert.deepEqual(error4, null);
                connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error5, result5) => {
                  assert.deepEqual(error5, null);
                  assert.deepEqual(result5.length, 0);
                  assert.deepEqual(result5.count, -1);
                  connection.close((error6) => {
                    assert.deepEqual(error6);
                    done();
                  });
                });
              });
            });
          });
        });
      });
    });
    it('...shouldn\'t rollback commits from before beginTransaction() was called.', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`, (error1, result1) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.count, 1);
          connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error2, result2) => {
            assert.deepEqual(error2, null);
            assert.deepEqual(result2.length, 1);
            assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
            assert.deepEqual(result2.count, -1);
            connection.beginTransaction((error3) => {
              assert.deepEqual(error3, null);
              connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(2, 'rolledback', 20)`, (error4, result4) => {
                assert.deepEqual(error4, null);
                assert.notDeepEqual(result4, null);
                assert.deepEqual(result4.count, 1);
                connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error5, result5) => {
                  assert.deepEqual(error5, null);
                  assert.deepEqual(result5.length, 2);
                  assert.deepEqual(result5[1], { ID: 2, NAME: 'rolledback', AGE: 20 });
                  assert.deepEqual(result5.count, -1);
                  connection.rollback((error6) => {
                    assert.deepEqual(error6, null);
                    connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error7, result7) => {
                      assert.deepEqual(error7, null);
                      assert.deepEqual(result7.length, 1);
                      assert.deepEqual(result7[0], { ID: 1, NAME: 'committed', AGE: 10 });
                      assert.deepEqual(result7.count, -1);
                      connection.close((error8) => {
                        assert.deepEqual(error8, null);
                        done();
                      });
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
    it('...shouldn\'t affect queries when beginTransaction() hasn\'t been called.', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`, (error1, result1) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(result1, null);
          assert.deepEqual(result1.count, 1);
          connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error2, result2) => {
            assert.deepEqual(error2, null);
            assert.deepEqual(result2.length, 1);
            assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
            assert.deepEqual(result2.count, -1);
            connection.rollback((error3) => {
              assert.deepEqual(error3, null);
              connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error4, result7) => {
                assert.deepEqual(error4, null);
                assert.deepEqual(result7.length, 1);
                assert.deepEqual(result7[0], { ID: 1, NAME: 'committed', AGE: 10 });
                assert.deepEqual(result7.count, -1);
                connection.close((error5) => {
                  assert.deepEqual(error5, null);
                  done();
                });
              });
            });
          });
        });
      });
    });
    it('...shouldn\'t rollback if called after a transaction is already ended with commit().', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        connection.beginTransaction((error1) => {
          assert.deepEqual(error1, null);
          connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`, (error2, result2) => {
            assert.deepEqual(error2, null);
            assert.notDeepEqual(result2, null);
            assert.deepEqual(result2.count, 1);
            connection.commit((error3) => {
              assert.deepEqual(error3, null);
              connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error4, result4) => {
                assert.deepEqual(error4, null);
                assert.deepEqual(result4.length, 1);
                assert.deepEqual(result4[0], { ID: 1, NAME: 'committed', AGE: 10 });
                assert.deepEqual(result4.count, -1);
                connection.rollback((error5) => {
                  assert.deepEqual(error5, null);
                  connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error6, result6) => {
                    assert.deepEqual(error6, null);
                    assert.deepEqual(result6.length, 1);
                    assert.deepEqual(result6[0], { ID: 1, NAME: 'committed', AGE: 10 });
                    assert.deepEqual(result6.count, -1);
                    connection.close((error7) => {
                      assert.deepEqual(error7, null);
                      done();
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
    it('...shouldn\'t rollback if called after a transaction is already ended with rollback().', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        connection.beginTransaction((error1) => {
          assert.deepEqual(error1, null);
          connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`, (error2, result2) => {
            assert.deepEqual(error2, null);
            assert.notDeepEqual(result2, null);
            assert.deepEqual(result2.count, 1);
            connection.rollback((error3) => {
              assert.deepEqual(error3, null);
              connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`, (error4, result4) => {
                assert.deepEqual(error4, null);
                assert.notDeepEqual(result4, null);
                assert.deepEqual(result4.count, 1);
                connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error5, result5) => {
                  assert.deepEqual(error5, null);
                  assert.deepEqual(result5.length, 1);
                  assert.deepEqual(result5[0], { ID: 1, NAME: 'committed', AGE: 10 });
                  assert.deepEqual(result5.count, -1);
                  connection.rollback((error6) => {
                    assert.deepEqual(error6, null);
                    connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error7, result7) => {
                      assert.deepEqual(error7, null);
                      assert.deepEqual(result7.length, 1);
                      assert.deepEqual(result7[0], { ID: 1, NAME: 'committed', AGE: 10 });
                      assert.deepEqual(result7.count, -1);
                      connection.close((error8) => {
                        assert.deepEqual(error8, null);
                        done();
                      });
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
  }); // '...with callbacks...'
  describe('...with promises...', () => {
    it('...should rollback all queries from after beginTransaction() was called.', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      await connection.beginTransaction();
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(2, 'rolledback', 20)`);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.count, 1);
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2.count, -1);
      assert.deepEqual(result2[0], { ID: 2, NAME: 'rolledback', AGE: 20 });
      await connection.rollback();
      const result3 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.deepEqual(result3.length, 0);
      assert.deepEqual(result3.count, -1);
      await connection.close();
    });
    it('...shouldn\'t rollback commits from before beginTransaction() was called.', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.count, 1);
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2.count, -1);
      assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
      await connection.beginTransaction();
      const result3 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(2, 'rolledback', 20)`);
      assert.notDeepEqual(result3, null);
      assert.deepEqual(result3.count, 1);
      const result4 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.deepEqual(result4.length, 2);
      assert.deepEqual(result4.count, -1);
      assert.deepEqual(result4[1], { ID: 2, NAME: 'rolledback', AGE: 20 });
      await connection.rollback();
      const result5 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.deepEqual(result5.length, 1);
      assert.deepEqual(result5.count, -1);
      assert.deepEqual(result5[0], { ID: 1, NAME: 'committed', AGE: 10 });
      await connection.close();
    });
    it('...shouldn\'t affect queries when beginTransaction() hasn\'t been called.', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.count, 1);
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
      assert.deepEqual(result2.count, -1);
      await connection.rollback();
      const result3 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.deepEqual(result3.length, 1);
      assert.deepEqual(result3[0], { ID: 1, NAME: 'committed', AGE: 10 });
      assert.deepEqual(result3.count, -1);
      await connection.close();
    });
    it('...shouldn\'t rollback if called after a transaction is already ended with a commit().', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      await connection.beginTransaction();
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.count, 1);
      await connection.commit();
      const result2 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.deepEqual(result2.length, 1);
      assert.deepEqual(result2[0], { ID: 1, NAME: 'committed', AGE: 10 });
      assert.deepEqual(result2.count, -1);
      await connection.rollback();
      const result3 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.deepEqual(result3.length, 1);
      assert.deepEqual(result3[0], { ID: 1, NAME: 'committed', AGE: 10 });
      assert.deepEqual(result3.count, -1);
      await connection.close();
    });
    it('...shouldn\'t rollback if called after a transaction is already ended with a rollback().', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      await connection.beginTransaction();
      const result1 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`);
      assert.notDeepEqual(result1, null);
      assert.deepEqual(result1.count, 1);
      await connection.rollback();
      const result2 = await connection.query(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'committed', 10)`);
      assert.notDeepEqual(result2, null);
      assert.deepEqual(result2.count, 1);
      const result3 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.deepEqual(result3.length, 1);
      assert.deepEqual(result3[0], { ID: 1, NAME: 'committed', AGE: 10 });
      assert.deepEqual(result3.count, -1);
      await connection.rollback();
      const result4 = await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      assert.deepEqual(result4.length, 1);
      assert.deepEqual(result4[0], { ID: 1, NAME: 'committed', AGE: 10 });
      assert.deepEqual(result4.count, -1);
      await connection.close();
    });
  }); // '...with promises...'
}); // '.rollback(callback)...'
