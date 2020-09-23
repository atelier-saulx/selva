import * as http from 'http'
import { ConnectOptions } from '@saulx/selva'
import mkHandlers, { Middleware } from './handler'
import * as url from 'url'

function createServer(
  selvaConnectOpts: ConnectOptions,
  middlewares?: Middleware[]
) {
  const handlers = mkHandlers(selvaConnectOpts, middlewares)

  const srv = http.createServer((req, res) => {
    const path = url.parse(req.url).pathname

    if (path === '/get') {
      return handlers.get(req, res)
    } else if (path === '/set') {
      return handlers.set(req, res)
    } else if (path === '/delete') {
      return handlers.delete(req, res)
    } else if (path === '/update_schema') {
      return handlers.updateSchema(req, res)
    } else {
      // pong
      req.pipe(res)
      res.statusCode = 200
    }
  })

  return srv
}

function start(
  selvaConnectOpts: ConnectOptions,
  middlewares?: Middleware[],
  port?: number
): () => void {
  const srv = createServer(selvaConnectOpts, middlewares)
  srv.listen(port)

  return () => {
    srv.close()
  }
}

export { mkHandlers as createHandlers, createServer, start }
