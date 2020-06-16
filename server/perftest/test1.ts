import { generateNSPTree } from './util/gen-tree';

const redis = require('redis')
const {promisify} = require('util')

redis.add_command('SELVA.id')
redis.add_command('SELVA.HIERARCHY.add')
const r = redis.createClient(6379, '127.0.0.1')

export default async function test1() {
    await generateNSPTree(r, 'test', 2, 1, 40, 0.1);
}
