var common = require("./common")
  , odbc = require("../")
  , db = new odbc.Database()
  , assert = require("assert");

db.openSync(common.connectionString);

var userName = db.conn.getInfoSync(odbc.SQL_USER_NAME);
console.log(userName, common.user, odbc.SQL_USER_NAME);
assert.equal(userName, common.user);

