'use strict';

var redis = require('redis');
var client = redis.createClient('/tmp/redis.sock');
var profiler = require('v8-profiler');

client.on('connect', function () {
    console.log('Got Unix socket connection.');
});

client.on('error', function (err) {
    console.log(err.message);
});

client.set('space chars', 'space value');

setInterval(function () {
    client.get('space chars');
}, 100);

function done () {
    client.info(function (err, reply) {
        console.log(reply.toString());
        client.quit();
    });
}

setTimeout(function () {
    console.log('Taking snapshot.');
    profiler.takeSnapshot();
    done();
}, 5000);
