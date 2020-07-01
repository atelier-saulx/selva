import * as net from 'net'
import * as fs from 'fs'

import { compile, deserialize } from 'data-record'

const SOCKET_PATH = '/tmp/selva.sock'

const STRUCT_DEF = compile(
  [
    { name: 'type', type: 'uint32_le' },
    { name: 'id', type: 'cstring', size: 10 },
    { name: 'fieldName', type: 'cstring_p' },
    { name: 'value', type: 'cstring_p' }
  ],
  { align: true }
)

try {
  fs.unlinkSync(SOCKET_PATH)
} catch (_e) {}

net
  .createServer(socket => {
    let size: number
    let buf: Buffer
    let got = 0

    const drain = () => {
      // console.log('DRAINING')
      if (!buf) {
        const sizeRaw: Buffer = socket.read(4)
        if (!sizeRaw) {
          return
        }

        size = sizeRaw.readInt32LE(0)
        // console.log('SIZE', size)
        buf = Buffer.alloc(size)
      }

      while (got < size) {
        const chunk: Buffer = socket.read(size - got)
        if (!chunk) {
          console.log('no chunk', got, size)
          return
        }

        // console.log('read', chunk.length)
        // console.log('read chunk', chunk.toString('hex'))

        chunk.copy(buf, got)
        got += buf.length
      }

      if (got === size) {
        const result = deserialize(STRUCT_DEF, buf)
        // console.log('RESULT', result)
        result.type = result.type === 0 ? 'publish' : 'ft.add'
        // console.log('RESULT', result)
        buf = undefined
        size = 0
        got = 0
        drain()
      } else {
        console.log('waiting for more data', got, size)
      }
    }

    socket.on('readable', drain)
  })
  .listen(SOCKET_PATH)
