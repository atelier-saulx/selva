const redis = require('redis');
import {promisify} from 'util';

redis.add_command('SELVA.id')
redis.add_command('SELVA.HIERARCHY.add')
redis.add_command('SELVA.HIERARCHY.dump')
redis.add_command('SELVA.HIERARCHY.findancestors')
const r = redis.createClient(6379, '127.0.0.1')

export default r;
