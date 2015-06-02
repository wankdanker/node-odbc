var common = require("./common")
  , odbc = require("../")
  , db = new odbc.Database()
  , assert = require("assert")
  , util = require('util')
  , count = 0
  ;
 
var sql = (common.dialect == 'sqlite' || common.dialect =='mysql')
  ? 'select cast(-1 as signed) as test, cast(-2147483648 as signed) as test2, cast(2147483647 as signed) as test3;'
  : 'select cast(-1 as int) as test, cast(-2147483648 as int) as test2, cast(2147483647 as int) as test3;'
  ;

db.open(common.connectionString, function(err) {
  console.error(err || "Connected")

  if (!err) {
    db.query(sql, function (err, results, more) {
      console.log(results);

      assert.equal(err, null);
      assert.equal(results[0].test, -1);
      assert.equal(results[0].test2, -2147483648);
      assert.equal(results[0].test3, 2147483647);

      db.close(function(err) { console.log(err || "Closed") })
    })
  }
});
