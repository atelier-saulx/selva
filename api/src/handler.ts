import { IncomingMessage, ServerResponse } from 'http'
import { json } from 'body-parser'
import { connect, ConnectOptions } from '@saulx/selva'

const jsonParser = json({ limit: '50mb' })

export default function(
  method: 'get' | 'set' | 'delete' | 'update_schema',
  connectOptions: ConnectOptions
): (req: IncomingMessage, res: ServerResponse) => void {
  if (
    method !== 'set' &&
    method !== 'get' &&
    method !== 'delete' &&
    method !== 'update_schema'
  ) {
    throw new Error(`Unsupported method ${method}`)
  }

  const client = connect(connectOptions)

  return (req: IncomingMessage, res: ServerResponse) => {
    if (req.method !== 'POST') {
      console.error(`Unsupported method ${req.method} on ${req.url}`)
      res.statusCode = 400
      res.end('Bad request')
      return
    }

    jsonParser(req, res, err => {
      if (err) {
        console.error('Error parsing request body', err)
        res.statusCode = 400
        res.end('Bad request')
        return
      }

      // note: populated by jsonParser
      const body: any = (<any>req).body
      if (method === 'get') {
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
      } else if (method === 'set') {
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
      } else if (method === 'delete') {
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
      } else {
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
      }
    })
  }
}
