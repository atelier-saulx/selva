import { Connection } from '.'

import { RedisClient } from 'redis'

const startClient = (
  connection: Connection,
  type: 'subscriber' | 'publisher'
): RedisClient => {
  let tries = 0
  let retryTimer = 0

  const retryStrategy = () => {
    tries++

    if (tries > 10) {
      if (!connection.isDestroyed) {
        connection.emit('hard-disconnect')
        connection.destroy()
      }
    }

    if (connection.clientsConnected[type] === true) {
      connection.clientsConnected[type] = false
      connection.connected = false
      connection.emit('disconnect', type)
    }

    tries++
    if (retryTimer < 1e3) {
      retryTimer += 100
    }
    // redisClient.emit('error', error)
    return retryTimer
  }

  const client = new RedisClient({
    host: connection.serverDescriptor.host,
    port: connection.serverDescriptor.port,
    retry_strategy: retryStrategy
  })

  client.on('ready', () => {
    connection.clientsConnected[type] = true
    for (const t in connection.clientsConnected) {
      if (connection.clientsConnected[t] === false) {
        return
      }
    }
    connection.connected = true
    connection.emit('connected')
  })

  client.on('error', err => {
    console.error('Error from node-redis', err.message)
  })

  return client
}

export default (connection: Connection) => {
  this.subscriber = startClient(connection, 'subscriber')
  this.publisher = startClient(connection, 'publisher')
}
