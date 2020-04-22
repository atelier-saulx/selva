import * as http from 'http'
import { ConnectOptions } from '@saulx/selva'
import mkHandlers from './handler'

function createServer(selvaConnectOpts: ConnectOptions) {
  const handlers = mkHandlers(selvaConnectOpts)

  const srv = http.createServer((req, res) => {
    if (req.url === '/get') {
      return handlers.get(req, res)
    } else if (req.url === '/set') {
      return handlers.set(req, res)
    } else if (req.url === '/delete') {
      return handlers.delete(req, res)
    } else if (req.url === '/update_schema') {
      return handlers.updateSchema(req, res)
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

export { mkHandlers as createHandlers, createServer, start }
