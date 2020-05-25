import { RedisClient } from 'redis'
import { Client } from './'

const createRedisClient = (
  client: Client,
  host: string,
  port: number,
  label: string
): RedisClient => {
  let tries = 0
  let retryTimer = 0
  let isConnected: boolean = false

  const retryStrategy = () => {
    if (tries > 200) {
      console.log('Node client is broken - restart')
      client.emit('node-redis-crash')
    } else {
      if (tries === 0 && isConnected === true) {
        isConnected = false
        // only send this once not twice (for each client type)
        if (label === 'publisher') {
          client.emit('disconnect', label)
        }
      }
    }
    tries++
    if (retryTimer < 1e3) {
      retryTimer += 100
    }
    return retryTimer
  }

  const redisClient = new RedisClient({
    port,
    host,
    retry_strategy: retryStrategy
  })

  redisClient.setMaxListeners(1e4)

  redisClient.on('ready', () => {
    tries = 0
    retryTimer = 0
    isConnected = true
    if (label === 'publisher') {
      client.emit('connect', label)
    }
  })

  redisClient.on('error', err => {
    if (err.code === 'ECONNREFUSED') {
      isConnected = false
      if (label === 'publisher') {
        client.emit('disconnect', label)
      }
    } else {
      client.emit('error', err)
    }
  })

  return redisClient
}

export default createRedisClient
