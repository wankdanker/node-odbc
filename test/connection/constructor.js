/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');
const { Connection } = require('../../lib/Connection');

describe('odbc.connect...', () => {
  describe('...with callbacks...', () => {
    it('...should return an open Connection when passed a valid connection string.', (done) => {
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
        assert.deepEqual(error1, null);
        assert.deepEqual(connection instanceof Connection, true);
        assert.deepEqual(connection.connected, true);
        assert.deepEqual(connection.autocommit, true);
        connection.close((error2) => {
          assert.deepEqual(error2, null);
          done();
        });
      });
    });
    it('...should throw an Error when passed an invalid connection string.', (done) => {
      odbc.connect('abc123!@#', (error, connection) => {
        assert.notDeepEqual(error, null);
        assert.deepEqual(connection, null);
        done();
      });
    });
  });
  describe('...with promises...', () => {
    it('...should return an open Connection when passed a valid connection string.', async () => {
      const connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
      assert.notDeepEqual(connection, null);
      // assert.deepEqual(connection instanceof odbc.Connection, true);
      assert.deepEqual(connection.connected, true);
      assert.deepEqual(connection.autocommit, true);
      await connection.close();
    });
    it('...should throw an Error when passed an invalid connection string.', async () => {
      assert.rejects(async () => {
        const connection = await odbc.connect('abc123!@#'); // eslint-disable-line no-unused-vars
      });
    });
  });
});
