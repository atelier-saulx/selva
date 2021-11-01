import { Connection } from '.'
import './redisClientExtensions'
import chalk from 'chalk'
import { SelvaClient } from '..'
import net from 'net'
import { SERVER_HEARTBEAT } from '../constants'

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

const createSocket = (
  connection: Connection,
  type: 'publisher' | 'subscriber'
) => {
  let tries = 0
  let retryTimer = 0
  let retryTimeout: NodeJS.Timeout

  const makeSocket = () => {
    if (connection[type]) {
      connection[type].removeAllListeners()
      connection[type].unref()
      delete connection[type]
    }

    if (connection.isDestroyed) {
      return
    }

    const socket = net.connect(connection.serverDescriptor)

    socket.setMaxListeners(10e3)

    socket.on('connect', () => {
      tries = 0
      // console.info(connection.serverDescriptor)
      console.info('connect SOCKET!', type, connection.serverDescriptor)

      connection.clientsConnected[type] = true
      for (const t in connection.clientsConnected) {
        if (connection.clientsConnected[t] === false) {
          return
        }
      }
      if (!connection.connected) {
        connection.connected = true
        clearTimeout(connection.startClientTimer)
        connection.startClientTimer = null
        connection.emit('connect')

        if (connection.isReconnect) {
          // may need this
          console.info('RECONN', connection.serverDescriptor.type)
          connection.clients.forEach((c) => {
            if (c instanceof SelvaClient) {
              c.emit('reconnect', connection.serverDescriptor)
            } else {
              c.reconnect(connection)
            }
          })
        } else {
          connection.isReconnect = true
        }
      }
    })

    socket.on('close', () => {
      // console.info('close! socket')
      retry()
    })

    socket.on('data', (d) => {
      console.info('DATAX!', d.toString())
    })

    socket.on('error', (err) => {
      // @ts-ignore
      if (err.code === 'ECONNREFUSED') {
        // console.info('cannot connect')
      } else {
        log(connection, type + ': ' + err.message)
      }
    })
    socket.on('drain', () => {
      console.info('drain time!')
    })
    connection[type] = socket
  }

  const retry = () => {
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
      connection.clientsConnected[type] = false
      if (connection.connected) {
        clearTimeout(connection.serverHeartbeatTimer)
        connection.connected = false
        connection.isDc = true
        console.info('DC SOCKET', connection.serverDescriptor.type)
        connection.emit('disconnect', type)
      }
    }
    tries++
    if (retryTimer < 1e3) {
      retryTimer += 100
    }
    // console.info('retry', tries)
    if (retryTimeout) {
      clearTimeout(retryTimeout)
    }
    retryTimeout = setTimeout(makeSocket, retryTimer)
  }

  makeSocket()
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

  createSocket(connection, 'subscriber')
  createSocket(connection, 'publisher')

  const serverHeartbeat = () => {
    clearTimeout(connection.serverHeartbeatTimer)
    connection.serverHeartbeatTimer = setTimeout(() => {
      if (!connection.isDestroyed) {
        log(
          connection,
          'Server heartbeat expired (longer then 1 min) destroy connection ' +
            connection.uuid
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

  // for internals this is pretty nice
  connection.subscribe(SERVER_HEARTBEAT, connection.uuid)
  connection.on('message', (channel) => {
    if (channel === SERVER_HEARTBEAT) {
      serverHeartbeat()
    }
  })
}
