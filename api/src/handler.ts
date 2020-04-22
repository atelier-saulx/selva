import { IncomingMessage, ServerResponse } from 'http'
import { json } from 'body-parser'
import { connect, ConnectOptions } from '@saulx/selva'

const jsonParser = json({ limit: '50mb' })

function checkPost(req: IncomingMessage, res: ServerResponse): boolean {
  if (req.method !== 'POST') {
    console.error(`Unsupported method ${req.method} on ${req.url}`)
    res.statusCode = 400
    res.end('Bad request')
    return false
  }

  return true
}

export default function(
  connectOptions: ConnectOptions
): {
  get: (req: IncomingMessage, res: ServerResponse) => void
  set: (req: IncomingMessage, res: ServerResponse) => void
  delete: (req: IncomingMessage, res: ServerResponse) => void
  updateSchema: (req: IncomingMessage, res: ServerResponse) => void
} {
  const client = connect(connectOptions)

  return {
    get: (req: IncomingMessage, res: ServerResponse) => {
      // TODO: middleware
      if (!checkPost(req, res)) {
        return
      }

      // TODO: middleware
      jsonParser(req, res, err => {
        if (err) {
          console.error('Error parsing request body', err)
          res.statusCode = 400
          res.end('Bad request')
          return
        }
        const body: any = (<any>req).body
        client
          .get(body)
          .then(result => {
            res.statusCode = 200
            res.setHeader('content-type', 'application/json')
            res.end(JSON.stringify(result))
          })
          .catch(e => {
            console.error('Error getting get response', e)
            res.statusCode = 500
            res.end('Internal server error')
            return
          })
      })
    },
    set: (req: IncomingMessage, res: ServerResponse) => {
      if (!checkPost(req, res)) {
        return
      }

      jsonParser(req, res, err => {
        if (err) {
          console.error('Error parsing request body', err)
          res.statusCode = 400
          res.end('Bad request')
          return
        }

        const body: any = (<any>req).body
        if (!body.$source) {
          body.$source = {
            $name: 'api',
            $overwrite: true
          }
        }
        client
          .set(body)
          .then(result => {
            if (!result) {
              console.error('Nothing was created', err)
              res.statusCode = 400
              res.end('Bad request')
              return
            }
            res.statusCode = 200
            res.setHeader('content-type', 'application/json')
            res.end(JSON.stringify({ id: result }))
          })
          .catch(e => {
            console.error('Error getting get response', e)
            res.statusCode = 500
            res.end('Internal server error')
            return
          })
      })
    },
    delete: (req: IncomingMessage, res: ServerResponse) => {
      if (!checkPost(req, res)) {
        return
      }

      jsonParser(req, res, err => {
        if (err) {
          console.error('Error parsing request body', err)
          res.statusCode = 400
          res.end('Bad request')
          return
        }

        const body: any = (<any>req).body

        client
          .delete(body)
          .then(isRemoved => {
            res.statusCode = 200
            res.setHeader('content-type', 'application/json')
            res.end(JSON.stringify({ isRemoved: !!isRemoved }))
          })
          .catch(e => {
            console.error('Error deleting', e)
            res.statusCode = 500
            res.end('Internal server error')
            return
          })
      })
    },
    updateSchema: (req: IncomingMessage, res: ServerResponse) => {
      if (!checkPost(req, res)) {
        return
      }

      jsonParser(req, res, err => {
        if (err) {
          console.error('Error parsing request body', err)
          res.statusCode = 400
          res.end('Bad request')
          return
        }

        const body: any = (<any>req).body
        client
          .updateSchema(body)
          .then(() => {
            res.statusCode = 200
            res.setHeader('content-type', 'application/json')
            res.end(JSON.stringify(client.schema))
          })
          .catch(e => {
            console.error('Error setting schema', e)
            res.statusCode = 500
            res.end('Internal server error ' + e.message)
            return
          })
      })
    }
  }
}
