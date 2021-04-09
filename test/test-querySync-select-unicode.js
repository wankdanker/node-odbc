var common = require("./common")
  , odbc = require("../")
  , db = new odbc.Database()
  , assert = require("assert")
  ;

db.openSync(common.connectionString);
var data;

try {
  data = db.querySync("select N'ꜨꜢ' as UNICODETEXT");
}
catch (e) {
  console.log(e.stack);
}

db.closeSync();
console.log(data);
assert.deepEqual(data, [{ UNICODETEXT: 'ꜨꜢ' }]);

