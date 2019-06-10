/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');

describe('.bind(parameters, [calback])...', () => {
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

    const BIND_TYPE_ERROR = {
      name: 'TypeError',
      message: '[node-odbc]: Incorrect function signature for call to statement.bind({array}, {function}[optional]).',
    };
    const DUMMY_CALLBACK = () => {};
    const PREPARE_SQL = `INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`;

    assert.throws(() => {
      statement.bind();
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.bind(DUMMY_CALLBACK);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.bind(PREPARE_SQL);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.bind(PREPARE_SQL, DUMMY_CALLBACK);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.bind(1);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.bind(1, DUMMY_CALLBACK);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.bind(null);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.bind(null, DUMMY_CALLBACK);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.bind(undefined);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.bind(undefined, DUMMY_CALLBACK);
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.bind({});
    }, BIND_TYPE_ERROR);
    assert.throws(() => {
      statement.bind({}, DUMMY_CALLBACK);
    }, BIND_TYPE_ERROR);

    // await connection.close();
  });
  describe('...with callbacks...', () => {
    it('...should bind if a valid SQL string has been prepared.', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.bind([1, 'bound1', 10], (error3) => {
            assert.deepEqual(error3, null);
            done();
          });
        });
      });
    });
    it('...should bind even if an parameter types are invalid for the prepared SQL statement', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.bind(['ID', 1, 'AGE'], (error3) => {
            assert.deepEqual(error3, null);
            done();
          });
        });
      });
    });
    it('...should not bind if an invalid SQL statement has been prepared (> 0 parameters).', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare('INSERT INTO dummy123.table456 VALUES(?, ?, ?)', (error2) => {
          assert.notDeepEqual(error2, null);
          assert.deepEqual(error2 instanceof Error, true);
          statement.bind([1, 'bound', 10], (error3) => {
            assert.notDeepEqual(error3, null);
            assert.deepEqual(error3 instanceof Error, true);
            done();
          });
        });
      });
    });
    it('...should not bind if an invalid SQL statement has been prepared (0 parameters).', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare('INSERT INTO dummy123.table456 VALUES(1, \'bound\', 10)', (error2) => {
          assert.notDeepEqual(error2, null);
          assert.deepEqual(error2 instanceof Error, true);
          statement.bind([], (error3) => {
            assert.notDeepEqual(error3, null);
            assert.deepEqual(error3 instanceof Error, true);
            done();
          });
        });
      });
    });
    it('...should not bind if there is an incorrect number of parameters for the prepared SQL statement', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
          assert.deepEqual(error2, null);
          statement.bind([1, 'bound'], (error3) => { // only 2 parameters, while statement expects 3.
            assert.notDeepEqual(error3, null);
            assert.deepEqual(error3 instanceof Error, true);
            done();
          });
        });
      });
    });
    it('...should not bind if statement.prepare({string}, {function}[optional]) has not yet been called (> 0 parameters).', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.bind([1, 'bound', 10], (error3) => {
          assert.notDeepEqual(error3, null);
          assert.deepEqual(error3 instanceof Error, true);
          done();
        });
      });
    });
    it('...should not bind if statement.prepare({string}, {function}[optional]) has not yet been called (0 parameters).', (done) => {
      connection.createStatement((error1, statement) => {
        assert.deepEqual(error1, null);
        assert.notDeepEqual(statement, null);
        statement.bind([], (error2) => {
          assert.notDeepEqual(error2, null);
          assert.deepEqual(error2 instanceof Error, true);
          done();
        });
      });
    });
  }); // '...with callbacks...'
  describe('...with promises...', () => {
    it('...should bind if a valid SQL string has been prepared.', async () => {
      await assert.doesNotReject(async () => {
        const statement = await connection.createStatement();
        assert.notDeepEqual(statement, null);
        await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
        await statement.bind([1, 'bound1', 10]);
      });
    });
    it('...should bind even if an parameter types are invalid for the prepared SQL statement', async () => {
      await assert.doesNotReject(async () => {
        const statement = await connection.createStatement();
        assert.notDeepEqual(statement, null);
        await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
        await statement.bind(['ID', 1, 'AGE']);
      });
    });
    it('...should not bind if an invalid SQL statement has been prepared (> 0 parameters).', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      try {
        await statement.prepare('INSERT INTO dummy123.table456 VALUES(?, ?, ?)');
      } catch (error) {
        // no-op, we know it will throw an error
      }
      assert.rejects(async () => {
        await statement.bind([1, 'bound', 10]);
      });
    });
    it('...should not bind if an invalid SQL statement has been prepared (0 parameters).', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      try {
        await statement.prepare('INSERT INTO dummy123.table456 VALUES()');
      } catch (error) {
        // no-op, we know it will throw an error
      }
      assert.rejects(async () => {
        await statement.bind([]);
      });
    });
    it('...should not bind if there is an incorrect number of parameters for the prepared SQL statement', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
      await assert.rejects(async () => {
        await statement.bind([1, 'bound']);
      });
    });
    it('...should not bind if statement.prepare({string}, {function}[optional]) has not yet been called (> 0 parameters).', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await assert.rejects(async () => {
        await statement.bind([1, 'bound', 10]);
      });
    });
    it('...should not bind if statement.prepare({string}, {function}[optional]) has not yet been called (0 parameters).', async () => {
      const statement = await connection.createStatement();
      assert.notDeepEqual(statement, null);
      await assert.rejects(async () => {
        await statement.bind([]);
      });
    });
  }); // '...with promises...'
});
