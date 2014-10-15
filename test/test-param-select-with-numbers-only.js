var common = require("./common")
  , odbc = require("../")
  , db = new odbc.Database()
  , assert = require("assert");


db.open(common.connectionString, function (err) {
  assert.equal(err, null);
  
  db.query("select ? as INTCOL1, ? as INTCOL2, ? as INTCOL3, ? as FLOATCOL4, ? as FLOATYINT"
    , [5, 3, 1, 1.23456789012345, 12345.000]
    , function (err, data, more) {
        db.close(function () {
          console.log(data);
          assert.equal(err, null);
          assert(Array.isArray(data), 'data is not an array')
          assert(typeof data[0] === 'object', 'data[0] is not an object');
          assert.strictEqual(data[0].INTCOL1, 5, 'INTCOL1 is wrong');
          assert.strictEqual(data[0].INTCOL2, 3, 'INTCOL2 is wrong');
          assert.strictEqual(data[0].INTCOL3, 1, 'INTCOL3 is wrong');
          assert.equal(Math.round(data[0].FLOATCOL4 * 1000000) / 1000000, 1.234568); // In case the database's precision is not stellar...
          assert.strictEqual(data[0].FLOATYINT, 12345, 'FLOATYINT is wrong');

          //assert.deepEqual(data, [{
          //  INTCOL1: 5,
          //  INTCOL2: 3,
          //  INTCOL3: 1,
          //  FLOATCOL4 : 1.23456789012345,
          //  FLOATYINT : 12345
          //}]);
        });
    });
});
