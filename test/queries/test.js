/* eslint-env node, mocha */

require('dotenv').config();
const assert = require('assert');
const odbc = require('../../');

describe('Queries...', () => {
  let connection = null;

  beforeEach(async () => {
    connection = await odbc.connect(`${process.env.CONNECTION_STRING}`);
  });

  afterEach(async () => {
    await connection.close();
    connection = null;
  });

  it('...should pass w1nk/node-odbc issue #54', async () => {
    const result = await connection.query('select cast(-1 as INTEGER) as test1, cast(-2147483648 as INTEGER) as test2, cast(2147483647 as INTEGER) as test3 from sysibm.sysdummy1');
    assert.notDeepEqual(result, null);
    assert.deepEqual(result.count, -1);
    assert.deepEqual(result.length, 1);
    assert.deepEqual(result[0].TEST1, -1);
    assert.deepEqual(result[0].TEST2, -2147483648);
    assert.deepEqual(result[0].TEST3, 2147483647);
  });
  it('...should pass w1nk/node-odbc issue #85', async () => {
    const result = await connection.query('select cast(-1 as INTEGER) as test1, cast(-2147483648 as INTEGER) as test2, cast(2147483647 as INTEGER) as test3 from sysibm.sysdummy1');
    assert.notDeepEqual(result, null);
    assert.deepEqual(result.count, -1);
    assert.deepEqual(result.length, 1);
    assert.deepEqual(result[0].TEST1, -1);
    assert.deepEqual(result[0].TEST2, -2147483648);
    assert.deepEqual(result[0].TEST3, 2147483647);
  });
});
