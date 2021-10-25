'use strict';

var assert = require('assert');
var config = require('./lib/config');
var helper = require('./helper');
var redis = config.redis;

if (process.platform === 'win32') {
    // TODO: Fix redis process spawn on windows
    return;
}

describe('rename commands', function () {
    before(function (done) {
        helper.stopRedis(function () {
            helper.startRedis('./conf/rename.conf', done);
        });
    });

    helper.allTests(function (parser, ip, args) {

        describe('using ' + parser + ' and ' + ip, function () {
            var client = null;

            beforeEach(function (done) {
                if (helper.redisProcess().spawnFailed()) return done();
                client = redis.createClient({
                    rename_commands: {
                        set: '807081f5afa96845a02816a28b7258c3',
                        GETRANGE: '9e3102b15cf231c4e9e940f284744fe0'
                    },
                    parser: parser
                });

                client.on('ready', function () {
                    client.flushdb(done);
                });
            });

            afterEach(function () {
                if (helper.redisProcess().spawnFailed()) return;
                client.end(true);
            });

            it('allows to use renamed functions', function (done) {
                if (helper.redisProcess().spawnFailed()) this.skip();

                client.set('key', 'value', function (err, reply) {
                    assert.strictEqual(reply, 'OK');
                });

                client.get('key', function (err, reply) {
                    assert.strictEqual(err.message, "ERR unknown command 'get'");
                    assert.strictEqual(err.command, 'GET');
                    assert.strictEqual(reply, undefined);
                });

                client.getrange('key', 1, -1, function (err, reply) {
                    assert.strictEqual(reply, 'alue');
                    assert.strictEqual(err, null);
                    done();
                });
            });

            it('should also work with batch', function (done) {
                if (helper.redisProcess().spawnFailed()) this.skip();

                client.batch([['set', 'key', 'value']]).exec(function (err, res) {
                    assert.strictEqual(res[0], 'OK');
                });

                var batch = client.batch();
                batch.getrange('key', 1, -1);
                batch.exec(function (err, res) {
                    assert(!err);
                    assert.strictEqual(res.length, 1);
                    assert.strictEqual(res[0], 'alue');
                    done();
                });
            });

            it('should also work with multi', function (done) {
                if (helper.redisProcess().spawnFailed()) this.skip();

                client.multi([['set', 'key', 'value']]).exec(function (err, res) {
                    assert.strictEqual(res[0], 'OK');
                });

                var multi = client.multi();
                multi.getrange('key', 1, -1);
                multi.exec(function (err, res) {
                    assert(!err);
                    assert.strictEqual(res.length, 1);
                    assert.strictEqual(res[0], 'alue');
                    done();
                });
            });

            it('should also work with multi and abort transaction', function (done) {
                if (helper.redisProcess().spawnFailed()) this.skip();

                var multi = client.multi();
                multi.get('key');
                multi.getrange('key', 1, -1, function (err, reply) {
                    assert.strictEqual(reply, 'alue');
                    assert.strictEqual(err, null);
                });
                multi.exec(function (err, res) {
                    assert(err);
                    assert.strictEqual(err.message, 'EXECABORT Transaction discarded because of previous errors.');
                    assert.strictEqual(err.errors[0].message, "ERR unknown command 'get'");
                    assert.strictEqual(err.errors[0].command, 'GET');
                    assert.strictEqual(err.code, 'EXECABORT');
                    assert.strictEqual(err.errors[0].code, 'ERR');
                    done();
                });
            });

            it('should also work prefixed commands', function (done) {
                if (helper.redisProcess().spawnFailed()) this.skip();

                client.end(true);
                client = redis.createClient({
                    rename_commands: {
                        set: '807081f5afa96845a02816a28b7258c3'
                    },
                    parser: parser,
                    prefix: 'baz'
                });
                client.set('foo', 'bar');
                client.keys('*', function (err, reply) {
                    assert.strictEqual(reply[0], 'bazfoo');
                    assert.strictEqual(err, null);
                    done();
                });
            });

        });
    });

    after(function (done) {
        if (helper.redisProcess().spawnFailed()) return done();
        helper.stopRedis(function () {
            helper.startRedis('./conf/redis.conf', done);
        });
    });
});
