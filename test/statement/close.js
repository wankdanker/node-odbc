/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');

describe('.close([calback])...', () => {
  let connection = null;

  beforeEach(async () => {
    connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
  });

  afterEach(async () => {
    await connection.close();
    connection = null;
  });

  it('...should throw a TypeError if function signature doesn\'t match accepted signatures.', async () => {
    const statement = await connection.createStatement();

    const EXECUTE_TYPE_ERROR = {
      name: 'TypeError',
      message: '[node-odbc]: Incorrect function signature for call to statement.close({function}[optional]).',
    };
    const DUMMY_CALLBACK = () => {};
    const SQL_STRING = `INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`;

    assert.throws(() => {
      statement.close(SQL_STRING);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.close(SQL_STRING, DUMMY_CALLBACK);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.close(1);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.close(1, DUMMY_CALLBACK);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.close(null);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.close(null, DUMMY_CALLBACK);
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.close({});
    }, EXECUTE_TYPE_ERROR);
    assert.throws(() => {
      statement.close({}, DUMMY_CALLBACK);
    }, EXECUTE_TYPE_ERROR);
  });
  describe('...with callbacks...', () => {
    it('...should close a newly created statement.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.close((error2) => {
          assert.deepEqual(error2, null);
          done();
        });
      });
    });
    it('...should close after a statement has been prepared with parameters.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.close((error3) => {
            assert.deepEqual(error3, null);
            done();
          });
        });
      });
    });
    it('...should close after a statement has been prepared without parameters.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'bound', 10)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.close((error3) => {
            assert.deepEqual(error3, null);
            done();
          });
        });
      });
    });
    it('...should close after a statement has been prepared and values bound.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.bind([1, 'bound', 10], (error3) => {
            assert.deepEqual(error3, null);
            statement.close((error4) => {
              assert.deepEqual(error4, null);
              done();
            });
          });
        });
      });
    });
    it('...should close after a statement has been prepared, bound, and executed successfully.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.bind([1, 'bound', 10], (error3) => {
            assert.deepEqual(error3, null);
            statement.execute((error4, result4) => {
              assert.deepEqual(error4, null);
              assert.notDeepEqual(result4, null);
              statement.close((error5) => {
                assert.deepEqual(error5, null);
                done();
              });
            });
          });
        });
      });
    });
    it('...should close after calling prepare with an error (bad sql prepared).', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare('abc123!@#', (error2) => {
          assert.notDeepEqual(error2, null);
          assert.deepEqual(error2 instanceof Error, true);
          statement.close((error4) => {
            assert.deepEqual(error4, null);
            done();
          });
        });
      });
    });
    it('...should close after calling execute with an error (did not bind parameters).', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.execute((error3, result3) => {
            assert.notDeepEqual(error3, null);
            assert.deepEqual(error3 instanceof Error, true);
            assert.deepEqual(result3, null);
            statement.close((error4) => {
              assert.deepEqual(error4, null);
              done();
            });
          });
        });
      });
    });
    it('...should error if trying to prepare after closing.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.close((error2) => {
          assert.deepEqual(error2, null);
          statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error3) => {
            assert.notDeepEqual(error3, null);
            assert.deepEqual(error3 instanceof Error, true);
            done();
          });
        });
      });
    });
    it('...should error if trying to bind after closing.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.close((error3) => {
            assert.deepEqual(error3, null);
            statement.bind([1, 'bound', 10], (error4) => {
              assert.notDeepEqual(error4, null);
              assert.deepEqual(error4 instanceof Error, true);
              done();
            });
          });
        });
      });
    });
    it('...should error if trying to execute after closing.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.bind([1, 'bound', 10], (error3) => {
            assert.deepEqual(error3, null);
            statement.close((error4) => {
              assert.deepEqual(error4, null);
              statement.execute((error5, result5) => {
                assert.notDeepEqual(error5, null);
                assert.deepEqual(error5 instanceof Error, true);
                assert.deepEqual(result5, null);
                done();
              });
            });
          });
        });
      });
    });
  }); // '...with callbacks...'
  describe('...with promises...', () => {
    it('...should close a newly created statement.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.close();
    });
    it('...should close after a statement has been prepared with parameters.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
      await statement.close();
    });
    it('...should close after a statement has been prepared without parameters.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(1, 'bound', 10)`);
      await statement.close();
    });
    it('...should close after a statement has been prepared and values bound.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
      await statement.bind([1, 'bound', 10]);
      await statement.close();
    });
    it('...should close after a statement has been prepared, bound, and executed successfully.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
      await statement.bind([1, 'bound', 10]);
      const result = await statement.execute();
      assert.notDeepEqual(result, null);
      await statement.close();
    });
    it('...should close after calling prepare with an error (bad sql prepared).', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await assert.rejects(async () => {
        await statement.prepare('abc123!@#');
      });
      await statement.close();
    });
    it('...should close after calling execute with an error (did not bind parameters).', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
      let result;
      await assert.rejects(async () => {
        result = await statement.execute();
      });
      assert.deepEqual(result, null);
      await statement.close();
    });
    it('...should error if trying to prepare after closing.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.close();
      await assert.rejects(async () => {
        await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
      });
    });
    it('...should error if trying to bind after closing.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
      await statement.close();
      await assert.rejects(async () => {
        await statement.bind([1, 'bound', 10]);
      });
    });
    it('...should error if trying to execute after closing.', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
      await statement.bind([1, 'bound', 10]);
      await statement.close();
      let result;
      await assert.rejects(async () => {
        result = await statement.execute();
      });
      assert.deepEqual(result, null);
    });
  }); // '...with promises...'
});
