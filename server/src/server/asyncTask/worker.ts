const SOCKET_PATH = '/tmp/selva.sock'

import * as net from 'net'
import * as fs from 'fs'

try {
  fs.unlinkSync(SOCKET_PATH)
} catch (_e) {}

net
  .createServer(socket => {
    socket.on('data', d => {
      console.log('DATA', d.toString())
    })
  })
  .listen(SOCKET_PATH)
