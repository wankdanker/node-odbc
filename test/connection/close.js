/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');

describe('.close([callback])...', () => {
  it('...should throw a TypeError if function signature doesn\'t match accepted signatures.', async () => {
    const CLOSE_TYPE_ERROR = {
      name: 'TypeError',
      message: '[node-odbc]: Incorrect function signature for call to connection.close({function}[optional]).',
    };

    const CLOSE_CALLBACK = () => {};

    const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
    assert.throws(() => {
      connection.close(1, []);
    }, CLOSE_TYPE_ERROR);
    assert.throws(() => {
      connection.close(1, CLOSE_CALLBACK);
    }, CLOSE_TYPE_ERROR);
    assert.throws(() => {
      connection.close({}, CLOSE_CALLBACK);
    }, CLOSE_TYPE_ERROR);
    assert.throws(() => {
      connection.close(null);
    }, CLOSE_TYPE_ERROR);
    assert.throws(() => {
      connection.close(null, CLOSE_CALLBACK);
    }, CLOSE_TYPE_ERROR);
    assert.throws(() => {
      connection.close('CLOSE');
    }, CLOSE_TYPE_ERROR);
    assert.throws(() => {
      connection.close('CLOSE', CLOSE_CALLBACK);
    }, CLOSE_TYPE_ERROR);
    await connection.close();
  });
  describe('...with callbacks...', () => {
    it('...should set .connected property to false.', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
        assert.deepEqual(connection.connected, true);
        connection.close((error2) => {
          assert.deepEqual(error2, null);
          assert.deepEqual(connection.connected, false);
          done();
        });
      });
    });
    it('...shouldn\'t allow queries after close() is called.', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
        assert.deepEqual(error1);
        connection.close((error2) => {
          assert.deepEqual(error2, null);
          connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`, (error3, result3) => {
            assert.notDeepEqual(error3, null);
            assert.deepEqual(error3 instanceof Error, true);
            assert.deepEqual(result3);
            done();
          });
        });
      });
    });
  }); // ...with callbacks...
  describe('...with promises...', () => {
    it('...should set .connected property to false.', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      assert.deepEqual(connection.connected, true);
      await connection.close();
      assert.deepEqual(connection.connected, false);
    });
    it('...shouldn\'t allow queries after close() is called.', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      await connection.close();
      assert.rejects(async () => {
        await connection.query(`SELECT * FROM ${process.env.DB_SCHEMA}.${process.env.DB_TABLE}`);
      });
    });
  }); // ...with promises...
}); // '.close([callback])...'
