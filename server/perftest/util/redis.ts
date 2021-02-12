const redis = require('redis')

redis.add_command('SELVA.id')
redis.add_command('SELVA.HIERARCHY.add')
redis.add_command('SELVA.HIERARCHY.dump')
redis.add_command('SELVA.HIERARCHY.find')
const r = redis.createClient(6379, '127.0.0.1')

export default r
