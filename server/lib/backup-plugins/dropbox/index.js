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
const node_fetch_1 = __importDefault(require("node-fetch"));
const fs_1 = require("fs");
const dropbox_1 = require("dropbox");
function mkBackupFn(opts, path, rdbFilePath) {
    return __awaiter(this, void 0, void 0, function* () {
        const dropbox = new dropbox_1.Dropbox(Object.assign(Object.assign({}, opts), { fetch: node_fetch_1.default }));
        return {
            sendBackup() {
                return __awaiter(this, void 0, void 0, function* () {
                    const content = yield fs_1.promises.readFile(rdbFilePath);
                    try {
                        yield dropbox.filesUpload({
                            path,
                            contents: content
                        });
                    }
                    catch (e) {
                        // const chunks: Buffer[] = []
                        // const body = await new Promise((resolve, reject) => {
                        //   e.response.body.on(
                        //     'data',
                        //     (chunk: Buffer) =>
                        //       chunks.push(chunk) && console.log(chunk.toString('utf8'))
                        //   )
                        //   e.response.body.on('error', reject)
                        //   e.response.body.on('end', () =>
                        //     resolve(Buffer.concat(chunks).toString('utf8'))
                        //   )
                        //   e.response.body.on('finish', () =>
                        //     resolve(Buffer.concat(chunks).toString('utf8'))
                        //   )
                        // })
                        // console.error(body)
                        throw e;
                    }
                });
            },
            loadBackup() {
                return __awaiter(this, void 0, void 0, function* () {
                    // TODO
                });
            }
        };
    });
}
exports.default = mkBackupFn;
//# sourceMappingURL=index.js.map