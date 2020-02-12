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
const s3api_1 = require("./s3api");
function cleanUpOldBackups(s3, bucketName, retentionInDays) {
    return __awaiter(this, void 0, void 0, function* () {
        const objects = yield s3.listObjects(bucketName);
        const oldBackups = objects.filter(object => {
            const validSince = new Date(Date.now() - 1000 * 60 * 60 * 12 * retentionInDays);
            return new Date(object.Key) < validSince;
        });
        yield Promise.all(oldBackups.map(object => {
            console.log(`Deleting object ${object.Key}`);
            // return s3.deleteObject(bucketName, object.Key)
        }));
    });
}
function mkBackupFn(opts) {
    return __awaiter(this, void 0, void 0, function* () {
        const { endpoint, backupRetentionInDays = 30, bucketName, config } = opts;
        const s3 = s3api_1.createApi(config, endpoint);
        yield s3.ensureBucket(bucketName, 'private');
        return {
            sendBackup(rdbFilePath) {
                return __awaiter(this, void 0, void 0, function* () {
                    const dstFilepath = new Date().toISOString();
                    yield s3.storeFile(bucketName, dstFilepath, rdbFilePath);
                    yield cleanUpOldBackups(s3, bucketName, backupRetentionInDays);
                });
            },
            loadBackup(rdbFilePath, rdbLastModified) {
                return __awaiter(this, void 0, void 0, function* () {
                    const objects = yield s3.listObjects(bucketName);
                    const latest = objects.reduce((max, o) => {
                        if (new Date(o.Key) > new Date(max.Key)) {
                            return o;
                        }
                        return max;
                    });
                    if (!rdbLastModified || new Date(latest.Key) > rdbLastModified) {
                        const body = yield s3.getObject(bucketName, latest.Key);
                        fs_1.promises.writeFile(rdbFilePath, body);
                    }
                });
            }
        };
    });
}
exports.default = mkBackupFn;
//# sourceMappingURL=index.js.map