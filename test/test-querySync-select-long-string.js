var common = require("./common")
  , odbc = require("../lib/odbc")
  , db = new odbc.Database()
  , assert = require("assert")
  , str = ''
  , length = 1000001;
  ;


var set = 'abcdefghijklmnopqrstuvwxyz';

for (var x = 0; x < length; x++) {
  str += set[x % set.length];
}

db.openSync(common.connectionString);
assert.strictEqual(db.connected, true);

var data = db.querySync("select '" + str + "' as LONG_STRING");

db.closeSync();

console.log('string length: %s, returned length: %s', str.length, data[0].LONG_STRING.length);

// assert.deepEqual(data, [{ LONG_STRING: str }]);


