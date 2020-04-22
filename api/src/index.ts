import * as http from 'http'
import { ConnectOptions } from '@saulx/selva'
import handler from './handler'

function createServer(selvaConnectOpts: ConnectOptions) {
  const getHandler = handler('get', selvaConnectOpts)
  const setHandler = handler('set', selvaConnectOpts)
  const delHandler = handler('delete', selvaConnectOpts)
  const schemaHandler = handler('update_schema', selvaConnectOpts)

  const srv = http.createServer((req, res) => {
    if (req.url === '/get') {
      return getHandler(req, res)
    } else if (req.url === '/set') {
      return setHandler(req, res)
    } else if (req.url === '/delete') {
      return delHandler(req, res)
    } else if (req.url === '/update_schema') {
      return schemaHandler(req, res)
    } else {
      // pong
      req.pipe(res)
      res.statusCode = 200
    }
  })

  return srv
}

function start(selvaConnectOpts: ConnectOptions, port?: number): () => void {
  const srv = createServer(selvaConnectOpts)
  srv.listen(port)

  return () => {
    srv.close()
  }
}

export { handler, createServer, start }
