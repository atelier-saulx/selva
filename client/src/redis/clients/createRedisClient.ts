import { RedisClient } from 'redis'
import { Client } from './'

const createRedisClient = (
  client: Client,
  host: string,
  port: number
): RedisClient => {
  // client for emitting stuff!
  // add reconn options on the redis clients!
  return new RedisClient({ port, host })
}

export default createRedisClient
