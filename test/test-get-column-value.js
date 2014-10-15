var common = require("./common")
  , odbc = require("../")
  , db = new odbc.Database()
  , assert = require("assert");

db.open(common.connectionString, function (err) {
    db.fetchMode = odbc.FETCH_NONE;

    function rand(length) {
        var buf = new Buffer(length);
        for (var i = 0; i < buf.length; i++)
            buf[i] = 1 + Math.floor(Math.random() * 255);
        return buf.toString("binary");
    }

    var x = rand(694853), y = rand(524288), z = rand(524289), w = null

    console.log("x.length:", x.length, x.substr(0,80));
    console.log("y.length:", y.length);
    console.log("z.length:", z.length);

    db.queryResult("select ?, ?, ?, null", [x, y, z, w], function (err, result, more) {
        assert.ifError(err);

        result.fetch(function(err, data) {
            assert.ifError(err);
            assert.equal(data, null);

            result.getColumnValue(0, function (err, xval, more) {
                assert.ifError(err);
                assert(!more, "more should be falsy when no maxBytes is passed");

                console.log("xval.length:", xval.length);

                result.getColumnValue(1, 2048*2+2, function (err, yval1, more) {
                    assert.ifError(err);
                    assert(more, "more should be truthy when maxBytes is less than the length of the string");

                    console.log(y.length, yval1.length)
                    assert.equal(yval1.length, 2048);

                    result.getColumnValue(1, function (err, yval2, more) {
                        assert.ifError(err);
                        assert(!more, "more should be falsy on the second call for y");

                        result.getColumnValue(2, function (err, zval, more) {
                            assert.ifError(err);
                            assert(!more, "more should be falsy for z");

                            result.getColumnValue(3, function (err, wval, more) {
                                assert.ifError(err);
                                assert(!more, "more should be falsy for w");

                                assert.equal(xval, x);
                                assert.equal(yval1, y.substr(0, 2048));
                                assert.equal(yval2, y.substr(2048, y.length - 2048));
                                assert.equal(zval, z);
                                assert.equal(wval, null);

                                console.log("Tests OK");
                                db.close(assert.ifError);
                            });
                        });
                    });
                });
            });
        });
    });
});

//    var sql = "declare @data varbinary(max) = 0x00112233445566778899aabbccddeeff, @n int = 4;\
//               while @n > 0 select @data += @data, @n -= 1;\
//               select datalength(@data) [length], @data [data], substring(@data,3,254) [data2] from (select 1 [x] union all select 2) rows;"

//    db.fetchMode = odbc.FETCH_NONE
//    db.queryResult(sql, function (err, result, more) {
//        console.log('db.queryResult callback:', err, result, more)

//        var row = 1;

//        function loop() {
//            result.fetch(function (err, _, more) {
//                console.log('result.fetch callback (row', row++ + '):', err, _, more)

//                assert.ifError(err)

//                if (more) {
//                    var bytes = result.getColumnValueSync(0), received = 0;
//                    console.log('Bytes to fetch:', bytes)

//                    console.log('Column 1 value: ', result.getColumnValueSync(1))

//                    result.getColumnValue(2, 0, function (err, res) {
//                        console.log(err, res)
//                        console.log(res.constructor.name);
//                        console.log(res.length);
//                        loop()
//                    })
//                } else {
//                    console.log('Test finished')
//                    db.close(function() {})
//                }
//            })
//        }

//        loop()
//    })
//})