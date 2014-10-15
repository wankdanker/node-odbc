var common = require("./common")
  , odbc = require("../")
  , db = new odbc.Database()
  , assert = require("assert")
  ;

assert.throws(function () {
  db.openSync("this is wrong");
});

assert.equal(db.connected, false);
  
db.open("this is wrong", function(err) {
  console.log(err);
  
  // unixODBC may return IM002, but ODBC on Windows gives you 01S00 (invalid connection string attribute)
  assert(err.state == 'IM002' || err.state == '01S00');
  
  assert.equal(db.connected, false);
});
