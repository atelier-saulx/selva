import { Connection, connections } from '.'
import { RedisClient } from 'redis'
import { SERVER_HEARTBEAT, LOG } from '../constants'
import './redisClientExtensions'
import chalk from 'chalk'
import { SelvaClient } from '..'

/*
    hard-disconnect
    Event to indicate that a redis client got corupted / cannot connect

    disconnect
    Before max retries on disconnection

    connect
    When both subscriber and publisher are connected
*/

const log = (connection: Connection, msg: string) => {
  const id = `${connection.serverDescriptor.host}:${connection.serverDescriptor.port}`
  console.error(
    chalk.red(
      `${msg} ${id} ${connection.serverDescriptor.type} ${connection.serverDescriptor.name}`
    )
  )
}

const startClient = (
  connection: Connection,
  type: 'subscriber' | 'publisher'
): RedisClient => {
  let tries = 0
  let retryTimer = 0
  let isReconnect = false

  const retryStrategy = () => {
    tries++
    if (tries > 30) {
      if (!connection.isDestroyed) {
        log(
          connection,
          'More then 30 retries to connect to server hard-disconnect'
        )
        connection.hardDisconnect()
      }
    }
    if (connection.clientsConnected[type] === true) {
      connection.serverHeartbeatTimer = null
      connection.clientsConnected[type] = false
      if (connection.connected) {
        clearTimeout(connection.serverHeartbeatTimer)
        connection.connected = false
        connection.isDc = true
        connection.emit('disconnect', type)
      }
    }
    tries++
    if (retryTimer < 1e3) {
      retryTimer += 100
    }
    return retryTimer
  }

  const client = new RedisClient({
    host: connection.serverDescriptor.host,
    port: connection.serverDescriptor.port,
    retry_strategy: retryStrategy
  })

  client.on('ready', () => {
    tries = 0

    connection.clientsConnected[type] = true
    for (const t in connection.clientsConnected) {
      if (connection.clientsConnected[t] === false) {
        return
      }
    }
    connection.connected = true
    clearTimeout(connection.startClientTimer)
    connection.startClientTimer = null
    connection.emit('connect')

    if (isReconnect) {
      connection.clients.forEach(c => {
        if (c instanceof SelvaClient) {
          c.emit('reconnect', connection.serverDescriptor)
        }
      })
    }

    isReconnect = true
  })

  client.on('error', err => {
    console.error('Error from node-redis', err.message)
  })

  client.on('hard-disconnect', () => {
    if (!connection.isDestroyed) {
      log(
        connection,
        'Strange info error node redis client is corrupt destroy connection'
      )
      connection.hardDisconnect()
    }
  })

  client.setMaxListeners(1e4)
  return client
}

export default (connection: Connection) => {
  connection.startClientTimer = setTimeout(() => {
    if (!connection.isDestroyed) {
      log(
        connection,
        'Took longer then 1 minute to connect to server destroy connection'
      )
      connection.hardDisconnect()
    }
  }, 60e3)

  connection.subscriber = startClient(connection, 'subscriber')
  connection.publisher = startClient(connection, 'publisher')

  const serverHeartbeat = () => {
    clearTimeout(connection.serverHeartbeatTimer)
    connection.serverHeartbeatTimer = setTimeout(() => {
      if (!connection.isDestroyed) {
        log(
          connection,
          'Server heartbeat expired (longer then 1 min) destroy connection'
        )
        connection.hardDisconnect()
      }
    }, 60e3)
  }

  connection.on(
    'connect',
    () => {
      serverHeartbeat()
    },
    'connection'
  )

  connection.subscriber.subscribe(SERVER_HEARTBEAT)

  connection.subscriber.on('message', channel => {
    if (channel === SERVER_HEARTBEAT) {
      serverHeartbeat()
    }
  })
}
