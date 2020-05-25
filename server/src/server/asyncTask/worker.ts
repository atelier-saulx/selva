const SOCKET_PATH = '/tmp/selva.sock'

import * as net from 'net'
import * as fs from 'fs'

try {
  fs.unlinkSync(SOCKET_PATH)
} catch (_e) {}

let i = 0
let currentLen = 0
let current: Buffer | undefined

net
  .createServer(socket => {
    socket.on('data', d => {
      if (!current) {
        currentLen = d.readInt32LE(0)
        console.log('FOUND NEW BUFFER WITH LEN', currentLen)
        current = new Buffer(currentLen)
        i = 0
      }

      d.copy(current, i, 0)
      i += d.byteLength
      currentLen -= d.byteLength

      console.log('CURRENT', currentLen)
      if (currentLen < 0) {
        console.log(current.toString())
        const newCurrentLen = d.readInt32LE(-currentLen - 1)
        current = new Buffer(newCurrentLen)
        d.copy(current, 0, -currentLen)
        i = d.byteLength + currentLen

        currentLen = newCurrentLen + currentLen
      } else if (currentLen === 0) {
        console.log(current.toString())
        current = undefined
      }
    })
  })
  .listen(SOCKET_PATH)
