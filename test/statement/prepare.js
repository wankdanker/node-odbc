/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');

describe('.prepare(sql, [calback])...', () => {
  it('...should throw a TypeError if function signature doesn\'t match accepted signatures.', async () => {
    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    const statement = await connection.createStatement();

    const PREPARE_TYPE_ERROR = {
      name: 'TypeError',
      message: '[node-odbc]: Incorrect function signature for call to statement.prepare({string}, {function}[optional]).',
    };
    const DUMMY_CALLBACK = () => {};

    assert.throws(() => {
      statement.prepare();
    }, PREPARE_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare(DUMMY_CALLBACK);
    }, PREPARE_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare(1);
    }, PREPARE_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare(1, DUMMY_CALLBACK);
    }, PREPARE_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare(null);
    }, PREPARE_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare(null, DUMMY_CALLBACK);
    }, PREPARE_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare(undefined);
    }, PREPARE_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare(undefined, DUMMY_CALLBACK);
    }, PREPARE_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare({});
    }, PREPARE_TYPE_ERROR);
    assert.throws(() => {
      statement.prepare({}, DUMMY_CALLBACK);
    }, PREPARE_TYPE_ERROR);

    // await connection.close();
  });
  describe('...with callbacks...', () => {
    it('...should prepare a valid SQL string', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.createStatement((error1, statement) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(statement, null);
          statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`, (error2) => {
            assert.deepEqual(error2, null);
            connection.close((error3) => {
              assert.deepEqual(error3, null);
              done();
            });
          });
        });
      });
    });
    it('...should return an error with an invalid SQL string', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.createStatement((error1, statement) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(statement, null);
          statement.prepare('INSERT INTO dummy123.table456 VALUES()', (error2) => {
            assert.notDeepEqual(error2, null);
            assert.deepEqual(error2 instanceof Error, true);
            connection.close((error3) => {
              assert.deepEqual(error3, null);
              done();
            });
          });
        });
      });
    });
    it('...should return an error with a blank SQL string', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error, connection) => {
        assert.deepEqual(error, null);
        connection.createStatement((error1, statement) => {
          assert.deepEqual(error1, null);
          assert.notDeepEqual(statement, null);
          statement.prepare('', (error2) => {
            assert.notDeepEqual(error2, null);
            assert.deepEqual(error2 instanceof Error, true);
            connection.close((error3) => {
              assert.deepEqual(error3, null);
              done();
            });
          });
        });
      });
    });
  }); // '...with callbacks...'
  describe('...with promises...', () => {
    it('...should prepare a valid SQL string', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const statement = await connection.createStatement();
      assert.doesNotReject(async () => {
        await statement.prepare(`INSERT INTO ${process.env.DB_SCHEMA}.${process.env.DB_TABLE} VALUES(?, ?, ?)`);
        await connection.close();
      });
    });
    it('...should return an error with an invalid SQL string', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const statement = await connection.createStatement();
      assert.rejects(async () => {
        await statement.prepare('INSERT INTO dummy123.table456 VALUES()');
        await connection.close();
      });
    });
    it('...should return an error with a blank SQL string', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      const statement = await connection.createStatement();
      assert.rejects(async () => {
        await statement.prepare('');
        await connection.close();
      });
    });
  }); // '...with promises...'
});
