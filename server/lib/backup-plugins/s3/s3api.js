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
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const fs_1 = require("fs");
const aws_sdk_1 = __importDefault(require("aws-sdk"));
function createApi(opts, endpoint) {
    if (!opts.accessKeyId || !opts.secretAccessKey) {
        throw new Error('No accessKeyId or secretAccessKey provided');
    }
    aws_sdk_1.default.config.update(opts);
    const s3 = new aws_sdk_1.default.S3({ endpoint: endpoint });
    const api = {
        getBuckets() {
            return new Promise((resolve, reject) => {
                s3.listBuckets((err, res) => {
                    if (err) {
                        return reject(err);
                    }
                    return resolve(res.Buckets);
                });
            });
        },
        createBucket(bucketName, acl) {
            return new Promise((resolve, reject) => {
                s3.createBucket({ Bucket: bucketName, ACL: acl }, (err, _res) => {
                    if (err) {
                        return reject(err);
                    }
                    resolve();
                });
            });
        },
        ensureBucket(bucketName, acl) {
            return __awaiter(this, void 0, void 0, function* () {
                const buckets = yield api.getBuckets();
                const found = buckets.find(bucket => {
                    return bucket.Name === bucketName;
                });
                if (found) {
                    return;
                }
                yield api.createBucket(bucketName, acl);
            });
        },
        listObjects(bucketName) {
            return new Promise((resolve, reject) => {
                s3.listObjects({ Bucket: bucketName }, (err, res) => {
                    if (err) {
                        return reject(err);
                    }
                    resolve(res.Contents);
                });
            });
        },
        getObject(bucketName, filepath) {
            return new Promise((resolve, reject) => {
                s3.getObject({ Bucket: bucketName, Key: filepath }, (err, res) => {
                    if (err) {
                        return reject(err);
                    }
                    resolve(res.Body);
                });
            });
        },
        deleteObject(bucketName, filepath) {
            return new Promise((resolve, reject) => {
                s3.deleteObject({ Bucket: bucketName, Key: filepath }, (err, _res) => {
                    if (err) {
                        return reject(err);
                    }
                    resolve();
                });
            });
        },
        storeFile(bucketName, destFilepath, sourceFilepath) {
            return __awaiter(this, void 0, void 0, function* () {
                const content = yield fs_1.promises.readFile(sourceFilepath);
                return new Promise((resolve, reject) => {
                    s3.upload({ Bucket: bucketName, Key: destFilepath, Body: content }, (err, _res) => {
                        if (err) {
                            return reject(err);
                        }
                        resolve();
                    });
                });
            });
        }
    };
    return api;
}
exports.createApi = createApi;
//# sourceMappingURL=s3api.js.map