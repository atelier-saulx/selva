"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const fs_1 = require("fs");
const redis_1 = require("redis");
const path_1 = require("path");
function roundUp(time, interval) {
    return Math.ceil(time / interval) * interval;
}
function msSinceMidnight() {
    const d = new Date();
    d.setHours(0, 0, 0, 0);
    return Date.now() - d.getTime();
}
function nextBackupTime(redis, backupInterval) {
    return __awaiter(this, void 0, void 0, function* () {
        const lastBackupTime = Number(yield new Promise((resolve, reject) => {
            redis.get('___selva_backup_timestamp', (err, reply) => {
                if (err)
                    return reject(err);
                resolve(reply);
            });
        }));
        return roundUp(lastBackupTime, backupInterval);
    });
}
function loadBackup(redisDir, backupFns) {
    return __awaiter(this, void 0, void 0, function* () {
        const dumpFile = path_1.join(redisDir, 'dump.rdb');
        try {
            const stat = yield fs_1.promises.stat(dumpFile);
            yield backupFns.loadBackup(dumpFile, stat.mtime);
        }
        catch (e) {
            yield backupFns.loadBackup(dumpFile);
        }
    });
}
exports.loadBackup = loadBackup;
// loads the latest backup, but only if it's newer than local dump.rdb
function saveAndBackUp(redisDir, redisPort, backupFns) {
    return __awaiter(this, void 0, void 0, function* () {
        const redis = redis_1.createClient({ port: redisPort });
        try {
            yield new Promise((resolve, reject) => {
                redis.save((err, reply) => {
                    if (err) {
                        return reject(err);
                    }
                    resolve(reply);
                });
            });
            yield backupFns.sendBackup(path_1.join(redisDir, 'dump.rdb'));
        }
        catch (e) {
            console.error(`Failed to back up ${e.stack}`);
            throw e;
        }
        finally {
            redis.end(false);
        }
    });
}
exports.saveAndBackUp = saveAndBackUp;
function scheduleBackups(redisDir, redisPort, intervalInMinutes, backupFns) {
    return __awaiter(this, void 0, void 0, function* () {
        yield new Promise((resolve, _reject) => setTimeout(resolve, 500));
        let redis = redis_1.createClient({ port: redisPort });
        const backupInterval = intervalInMinutes * 60 * 1000;
        while (true) {
            if (!redis) {
                redis = redis_1.createClient({ port: redisPort });
            }
            const nextBackup = yield nextBackupTime(redis, backupInterval);
            const timeOfDay = msSinceMidnight();
            if (timeOfDay >= nextBackup) {
                try {
                    yield backupFns.sendBackup(path_1.join(redisDir, 'dump.rdb'));
                    yield new Promise((resolve, reject) => {
                        redis.set('___selva_backup_timestamp', String(timeOfDay), (err, reply) => {
                            if (err)
                                return reject(err);
                            resolve(reply);
                        });
                    });
                }
                catch (e) {
                    console.error(`Failed to back up ${e}`);
                }
            }
            else {
                const delay = Math.max(nextBackup - timeOfDay, 1000 * 60); // wait at least 1 minute in between runs
                if (delay > 5 * 60 * 1000) {
                    // if we wait more than 5 minutes, we should probably close and reopen the redis connection
                    redis.end(false);
                    redis = null;
                }
                yield new Promise((resolve, _reject) => setTimeout(resolve, delay));
            }
        }
    });
}
exports.scheduleBackups = scheduleBackups;
//# sourceMappingURL=backups.js.map