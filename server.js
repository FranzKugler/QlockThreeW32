var express = require('express');
var request = require('request');
var bodyParser = require('body-parser');

var app = express();
app.use(express.static('data'));

var display = 1;

var hue = 257;
var sat = 72; //in %
var lum = 52;  //in %
var automaticLum = false;

var language = 2;
var cornerColor = 0;
var cornerDirection = 1;

var NTPServer = "pool.ntp.org";
var UseDs = true;
var TzName = "CET";
var TzWeek = 0;
var TzDoW = 1;
var TzMonth = 10;
var TzHour = 3;
var TzOffset = 60;

var TzDsName = "CEST";
var TzDsWeek = 0;
var TzDsDoW = 1;
var TzDsMonth = 3;
var TzDsHour = 2;
var TzDsOffset = 120;


app.use(bodyParser.json()); // support json encoded bodies
app.use(bodyParser.urlencoded({ extended: true })); // support encoded bodies

app.get('/currentState', function (req, res) {
	var obj = {};
	obj.display = display;
	obj.hue = hue;
	obj.sat = sat;
	obj.lum = lum;
	obj.automaticLum = automaticLum;
	obj.language = language;
	obj.cornerColor = cornerColor;
	obj.cornerDirection = cornerDirection;

	obj.ntpServer = NTPServer;
	obj.useDs = UseDs;
	obj.tzName = TzName;
	obj.tzWeek = TzWeek;
	obj.tzDoW = TzDoW;
	obj.tzMonth = TzMonth;
	obj.tzHour = TzHour;
	obj.tzOffset = TzOffset;
	obj.tzDsName = TzDsName;
	obj.tzDsWeek = TzDsWeek;
	obj.tzDsDoW = TzDsDoW;
	obj.tzDsMonth = TzDsMonth;
	obj.tzDsHour = TzDsHour;
	obj.tzDsOffset = TzDsOffset;

	res.header('Content-type', 'application/json');
	res.header('Charset', 'utf8');
	res.send(req.query.callback + '(' + JSON.stringify(obj) + ');');
	console.log(obj);
});

app.post('/display', function (req, res) {
	console.log(req.body);
	display = req.body.display;
	res.send({ msg: '' });
});

app.post('/autoluminance', function (req, res) {
	console.log(req.body);
	automaticLum = req.body.automaticLum;
	res.send({ msg: '' });

}); app.post('/color', function (req, res) {
	console.log(req.body);
	hue = req.body.hue;
	sat = req.body.sat;
	lum = req.body.lum;
	automaticLum = req.body.automaticLum;
	res.send({ msg: '' });
});

app.post('/configuration', function (req, res) {
	console.log(req.body);
	language = req.body.language;
	cornerColor = req.body.cornerColor;
	cornerDirection = req.body.cornerDirection;
	res.send({ msg: '' });
});

app.post('/timezone', function (req, res) {
	console.log(req.body);
	NTPServer = req.body.ntpServer;
	UseDs = req.body.useDs;
	TzName = req.body.tzName;
	TzWeek = req.body.tzWeek;
	TzDoW = req.body.tzDoW;
	TzMonth = req.body.tzMonth;
	TzHour = req.body.tzHour;
	TzOffset = req.body.tzOffset;
	
	TzDsName = req.body.tzDsName;
	TzDsWeek = req.body.tzDsWeek;
	TzDsDoW = req.body.tzDsDoW;
	TzDsMonth = req.body.tzDsMonth;
	TzDsHour = req.body.tzDsHour;
	TzDsOffset = req.body.tzDsOffset;
	res.send({ msg: '' });
});

app.listen(8080);