var common = require("./common")
  , assert = require("assert");

var Database = require('../').Database;

var cn = common.connectionString;

var db = new Database ();

db.open (cn, function (err) {
	if (err)
		return console.log (err);

	db.query('select * from ' + common.tableName, function (err, data) {
		console.log(arguments);
	
		db.close (function () {
			console.log ("Database connection closed");
		});
	})
});	

