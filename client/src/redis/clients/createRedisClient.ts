import { RedisClient } from 'redis'
import { Client } from './'
import { SERVER_HEARTBEAT } from '../../constants'

const createRedisClient = (
  client: Client,
  host: string,
  port: number,
  label: string
): RedisClient => {
  let tries = 0
  let retryTimer = 0
  let isConnected: boolean = false

  if (label === 'publisher') {
    client.startClientTimer = setTimeout(() => {
      console.log('cannot get it ready!', host, port, label)
      client.emit('hard-disconnect')
    }, 30e3)
  }

  const retryStrategy = () => {
    if (tries > 20) {
      console.log('HARD DC')
      clearTimeout(client.serverHeartbeat)
      clearTimeout(client.startClientTimer)
      if (label === 'publisher') {
        client.emit('hard-disconnect')
      }
    } else {
      if (tries === 0 && isConnected === true) {
        isConnected = false
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

  if (label === 'subscriber') {
    redisClient.on('message', channel => {
      if (channel === SERVER_HEARTBEAT) {
        clearTimeout(client.serverHeartbeat)
        client.serverHeartbeat = setTimeout(() => {
          console.log('heart beat expired disconnect it!')
          client.emit('hard-disconnect')
        }, 30e3)
      }
    })
  }

  redisClient.setMaxListeners(1e4)

  redisClient.on('ready', () => {
    console.log('is ready clear start timer')
    if (label === 'publisher') {
      clearTimeout(client.startClientTimer)
    }
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
