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
const redis_1 = require("redis");
const selva_client_1 = require("selva-client");
const crypto_1 = require("crypto");
function isObjectLike(x) {
    return !!(x && x.properties);
}
function makeAll(path, schema, opts) {
    const newOpts = Object.assign({}, opts);
    delete newOpts.$all;
    const parts = path.split('.');
    if (!newOpts.$id) {
        return newOpts;
    }
    const typeName = schema.prefixToTypeMapping[newOpts.$id.substr(0, 2)];
    const type = schema.types[typeName];
    if (!type) {
        return newOpts;
    }
    let prop = {
        type: 'object',
        properties: type.fields
    };
    for (let i = 0; i < parts.length; i++) {
        if (!parts[i]) {
            break;
        }
        if (!isObjectLike(prop)) {
            break;
        }
        else {
            prop = prop.properties[parts[i]];
        }
    }
    if (isObjectLike(prop)) {
        for (const propName in prop.properties) {
            newOpts[propName] = true;
        }
    }
    else if (prop.type === 'text') {
        for (const lang of schema.languages) {
            newOpts[lang] = true;
        }
    }
    return newOpts;
}
function addFields(path, fields, schema, opts) {
    let hasKeys = false;
    for (const key in opts) {
        if (key[0] === '$') {
            if (key === '$all') {
                addFields(path, fields, schema, makeAll(path, schema, opts));
                return;
            }
            else if (key === '$inherit') {
                fields.add('.ancestors');
                return;
            }
            else if (key === '$field') {
                if (Array.isArray(opts.$field)) {
                    opts.$field.forEach(f => fields.add('.' + f));
                }
                else {
                    fields.add('.' + opts.$field);
                }
                return;
            }
            // FIXME: other special options missing? -- $ref needs to be handled on lua side
            continue;
        }
        hasKeys = true;
        if (opts[key] === true) {
            fields.add(`${path}.${key}`);
        }
        else if (typeof opts[key] === 'object') {
            addFields(`${path}.${key}`, fields, schema, opts[key]);
        }
    }
    // default to adding the field if only options are specified
    if (!hasKeys) {
        fields.add(path);
    }
}
class SubscriptionManager {
    constructor() {
        this.subscriptions = {};
        this.subscriptionsByField = {};
        this.refsById = {};
        this.lastResultHash = {};
        this.lastHeartbeat = {};
    }
    heartbeats() {
        this.pub.publish('___selva_events:heartbeat', '');
        for (const subscriptionId in this.subscriptions) {
            this.pub.publish(`___selva_subscription:${subscriptionId}`, JSON.stringify({ type: 'heartbeat' }));
        }
    }
    attach(port) {
        return __awaiter(this, void 0, void 0, function* () {
            this.client = new selva_client_1.SelvaClient({ port }, { loglevel: 'off' });
            this.sub = redis_1.createClient({ port });
            this.pub = redis_1.createClient({ port });
            this.sub.on('error', e => {
                // console.error(e)
            });
            this.pub.on('error', e => {
                // console.error(e)
            });
            let tm;
            try {
                yield Promise.race([
                    new Promise((resolve, _reject) => {
                        let count = 0;
                        this.pub.on('ready', () => {
                            count++;
                            if (count === 2) {
                                if (tm) {
                                    clearTimeout(tm);
                                }
                                resolve();
                            }
                        });
                        this.sub.on('ready', () => {
                            count++;
                            if (count === 2) {
                                if (tm) {
                                    clearTimeout(tm);
                                }
                                resolve();
                            }
                        });
                    }),
                    new Promise((_resolve, reject) => {
                        tm = setTimeout(() => {
                            this.pub.removeAllListeners('ready');
                            this.sub.removeAllListeners('ready');
                            reject();
                        }, 5000);
                    })
                ]);
            }
            catch (e) {
                setTimeout(() => {
                    this.attach(port);
                }, 1000);
            }
            yield this.refreshSubscriptions();
            this.recalculateUpdates();
            // client heartbeat events
            this.sub.on('message', (_channel, message) => {
                const payload = JSON.parse(message);
                const subId = payload.channel.slice('___selva_subscription:'.length);
                this.lastHeartbeat[subId] = Date.now();
                if (payload.refresh) {
                    delete this.lastResultHash[subId];
                    this.refreshSubscription(subId)
                        .then(() => {
                        return this.sendUpdate(subId);
                    })
                        .catch(e => {
                        console.error(e);
                    });
                }
            });
            // lua object change events
            this.sub.on('pmessage', (_pattern, channel, message) => {
                this.lastModifyEvent = Date.now();
                if (channel === '___selva_events:heartbeat') {
                    return;
                }
                // used to deduplicate events for subscriptions,
                // firing only once if multiple fields in subscription are changed
                const updatedSubscriptions = {};
                const eventName = channel.slice('___selva_events:'.length);
                if (message === 'delete') {
                    for (const field in this.subscriptionsByField) {
                        if (field.startsWith(eventName)) {
                            const subscriptionIds = this.subscriptionsByField[field] || new Set();
                            for (const subscriptionId of subscriptionIds) {
                                if (updatedSubscriptions[subscriptionId]) {
                                    continue;
                                }
                                updatedSubscriptions[subscriptionId] = true;
                                this.sendUpdate(subscriptionId, null, true);
                            }
                        }
                    }
                    return;
                }
                else if (message === 'update') {
                    const parts = eventName.split('.');
                    let field = parts[0];
                    for (let i = 0; i < parts.length; i++) {
                        const subscriptionIds = this.subscriptionsByField[field] || new Set();
                        for (const subscriptionId of subscriptionIds) {
                            if (updatedSubscriptions[subscriptionId]) {
                                continue;
                            }
                            updatedSubscriptions[subscriptionId] = true;
                            this.sendUpdate(subscriptionId).catch(e => {
                                console.error(e);
                            });
                        }
                        field += '.' + parts[i + 1];
                    }
                }
            });
            this.sub.psubscribe('___selva_events:*');
            this.sub.subscribe('___selva_subscription:client_heartbeats');
            const timeout = () => {
                if (Date.now() - this.lastModifyEvent > 1000 * 30) {
                    this.detach();
                    this.attach(port).catch(e => {
                        console.error(e);
                    });
                    return;
                }
                this.heartbeats();
                this.refreshSubscriptions()
                    .catch(e => {
                    console.error(e);
                })
                    .finally(() => {
                    this.refreshSubscriptionsTimeout = setTimeout(timeout, 1000 * 10);
                });
            };
            timeout();
        });
    }
    detach() {
        this.sub.end(false);
        this.sub = undefined;
        this.pub.end(false);
        this.pub = undefined;
        if (this.refreshSubscriptionsTimeout) {
            clearTimeout(this.refreshSubscriptionsTimeout);
            this.refreshSubscriptionsTimeout = undefined;
        }
        this.lastRefreshed = undefined;
        this.lastHeartbeat = {};
    }
    get closed() {
        return this.sub === undefined;
    }
    sendUpdate(subscriptionId, getOptions, deleteOp = false) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!this.pub) {
                return;
            }
            if (deleteOp) {
                this.pub.publish(`___selva_subscription:${subscriptionId}`, JSON.stringify({ type: 'delete' }), (err, _reply) => {
                    console.error(err);
                });
                // delete cache for latest result since there is no result now
                delete this.lastResultHash[subscriptionId];
                return;
            }
            getOptions = getOptions || this.subscriptions[subscriptionId];
            const payload = yield this.client.get(Object.assign({}, getOptions, {
                $includeMeta: true
            }));
            const refs = payload.$meta.$refs;
            delete this.refsById[getOptions.$id];
            let hasRefs = false;
            const newRefs = {};
            for (const refSource in refs) {
                hasRefs = true;
                const refTargets = refs[refSource];
                newRefs[refSource] = refTargets;
            }
            this.refsById[getOptions.$id] = newRefs;
            if (hasRefs) {
                this.refreshSubscription(subscriptionId);
            }
            // clean up refs before we send it to the client
            delete payload.$meta;
            // hack-ish thing: include the result object in the string
            // so we don't need to encode/decode as many times
            const resultStr = JSON.stringify({ type: 'update', payload });
            const currentHash = this.lastResultHash[subscriptionId];
            const hashingFn = crypto_1.createHash('sha256');
            hashingFn.update(resultStr);
            const newHash = hashingFn.digest('hex');
            // de-duplicate events
            // with this we can avoid sending events where nothing changed upon reconnection
            // both for queries and for gets by id
            if (currentHash && currentHash === newHash) {
                return;
            }
            this.lastResultHash[subscriptionId] = newHash;
            this.pub.publish(`___selva_subscription:${subscriptionId}`, resultStr, (err, _reply) => {
                console.error(err);
            });
        });
    }
    refreshSubscription(subId, subs = this.subscriptions, fieldMap = this.subscriptionsByField, schema, stored, cleanup = false) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!schema) {
                schema = (yield this.client.getSchema()).schema;
            }
            if (!stored) {
                stored = yield this.client.redis.hget('___selva_subscriptions', subId);
            }
            const getOptions = JSON.parse(stored);
            if (cleanup && this.lastHeartbeat[subId]) {
                // if no heartbeats in two minutes, clean up
                if (Date.now() - this.lastHeartbeat[subId] > 1000 * 120) {
                    delete this.lastHeartbeat[subId];
                    delete this.lastResultHash[subId];
                    yield this.client.redis.hdel('___selva_subscriptions', subId);
                    return;
                }
            }
            const fields = new Set();
            subs[subId] = getOptions;
            addFields('', fields, schema, getOptions);
            for (const field of fields) {
                let current = fieldMap[getOptions.$id + field];
                if (!current) {
                    fieldMap[getOptions.$id + field] = current = new Set();
                }
                current.add(subId);
            }
            if (this.refsById[getOptions.$id]) {
                for (const refSource in this.refsById[getOptions.$id]) {
                    let current = fieldMap[getOptions.$id + '.' + refSource];
                    if (!current) {
                        fieldMap[getOptions.$id + '.' + refSource] = current = new Set();
                    }
                    current.add(subId);
                }
            }
        });
    }
    recalculateUpdates() {
        for (const subId in this.subscriptions) {
            this.sendUpdate(subId).catch(e => {
                console.error(e);
            });
        }
    }
    refreshSubscriptions() {
        return __awaiter(this, void 0, void 0, function* () {
            const schema = (yield this.client.getSchema()).schema;
            const lastEdited = yield this.client.redis.hget('___selva_subscriptions', '___lastEdited');
            // only refresh if there are new changes to the subscription metadata
            if (lastEdited && this.lastRefreshed) {
                const d = new Date(lastEdited);
                if (d <= this.lastRefreshed) {
                    return;
                }
            }
            const stored = yield this.client.redis.hgetall('___selva_subscriptions');
            if (!stored) {
                return;
            }
            const fieldMap = {};
            const subs = {};
            for (const subscriptionId in stored) {
                if (subscriptionId.startsWith('___')) {
                    // skip internal keys
                    continue;
                }
                this.refreshSubscription(subscriptionId, subs, fieldMap, schema, stored[subscriptionId], true);
            }
            this.lastRefreshed = new Date();
            this.subscriptionsByField = fieldMap;
            this.subscriptions = subs;
        });
    }
}
exports.default = SubscriptionManager;
//# sourceMappingURL=index.js.map