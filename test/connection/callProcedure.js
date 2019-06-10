/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');

describe.skip('.callProcedure(procedureName, parameters, [callback])...', () => {
  describe('...with callbacks...', () => {
    it('...should place correct result in an out parameter.', (done) => {
      const array = [undefined];
      odbc.connect(`${process.env.CONNECTION_STRING}`, (error1, connection) => {
        assert.deepEqual(error1, null);
        connection.callProcedure(null, `${process.env.DB_SCHEMA}`, `${process.env.DB_STOREDPROCEDURE}`, array, (error2, result2) => {
          assert.deepEqual(error2, null);
          assert.notDeepEqual(result2, null);
          done();
        });
      });
    });
  });
});
