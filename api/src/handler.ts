import { IncomingMessage, ServerResponse } from 'http'
import { json } from 'body-parser'
import { connect, ConnectOptions, SelvaClient, GetOptions } from '@saulx/selva'
import * as url from 'url'
import { SetOptions } from '@saulx/selva/dist/src/set'

export type MiddlewareNext = (proceed: boolean) => void

export type Middleware = (
  client: SelvaClient,
  req: IncomingMessage,
  res: ServerResponse,
  next: MiddlewareNext
) => void

const jsonParser = json({ limit: '50mb' })

function sendError(
  res: ServerResponse,
  error: Error,
  code: number = 500
): void {
  res.setHeader('content-type', 'application/json')
  res.statusCode = code

  res.end(JSON.stringify({ error: error.message }))
}

function checkPost(
  _client: SelvaClient,
  req: IncomingMessage,
  res: ServerResponse,
  next: MiddlewareNext
): void {
  if (req.method !== 'POST') {
    console.error(`Unsupported method ${req.method} on ${req.url}`)
    res.statusCode = 400
    res.end('Bad request')
    return next(false)
  }

  next(true)
}

export function constructGuard(setOpts: SetOptions): GetOptions {
  const result: GetOptions = {}
  for (const key in setOpts) {
    if (key.startsWith('$')) {
      continue
    }

    result[key] = true
  }

  // we use this elsewhere so always get id
  result.id = true

  if (setOpts.$alias) {
    result.$alias = setOpts.$alias
  } else if (setOpts.$id) {
    result.$id = setOpts.$id
  }

  if (setOpts.$language) {
    result.$language = setOpts.$language
  }

  return result
}

export function noHasGuard(setOpts: any, result: any): boolean {
  if (Array.isArray(setOpts) && Array.isArray(result)) {
    if (setOpts.length !== result.length) {
      return false
    }

    setOpts.sort()
    result.sort()

    for (let i = 0; i < setOpts.length; i++) {
      if (setOpts[i] !== result[i]) {
        return false
      }
    }

    return true
  } else if (typeof setOpts === 'object' && (setOpts.$add || setOpts.$delete)) {
    if (setOpts.$add) {
      const asSet = new Set(result)
      for (const val of setOpts.$add) {
        if (!asSet.has(val)) {
          return false
        }
      }
    }

    if (setOpts.$delete) {
      const asSet = new Set(result)
      for (const val of setOpts.$delete) {
        if (asSet.has(val)) {
          return false
        }
      }
    }

    return true
  } else if (
    typeof setOpts === 'object' &&
    setOpts.$value &&
    Array.isArray(result)
  ) {
    setOpts = setOpts.$value

    if (setOpts.length !== result.length) {
      return false
    }

    setOpts.sort()
    result.sort()

    for (let i = 0; i < setOpts.length; i++) {
      if (setOpts[i] !== result[i]) {
        return false
      }
    }

    return true
  } else if (typeof setOpts === 'object' && typeof result === 'object') {
    return Object.entries(setOpts).every(([key, val]) => {
      if (key.startsWith('$')) {
        // skip
        return true
      }

      return noHasGuard(val, result[key])
    })
  } else {
    if (typeof setOpts === 'object' && isObjectChildrenValueEmpty(setOpts)) {
      setOpts = undefined
    }

    return setOpts === result
  }
}

function isObjectChildrenValueEmpty(object) {
  return Object.values(object).every(val => (val === null || val === '' || (val.constructor === Object && isObjectChildrenValueEmpty(val))))
}

function parseJson(
  _client: SelvaClient,
  req: IncomingMessage,
  res: ServerResponse,
  next: MiddlewareNext
): void {
  jsonParser(req, res, err => {
    if (err) {
      console.error('Error parsing request body', err)
      res.statusCode = 400
      res.end('Bad request')
      return next(false)
    }

    next(true)
  })
}

function applyMiddleware(
  client: SelvaClient,
  middleware: Middleware[],
  handler: (req: IncomingMessage, res: ServerResponse) => void
): (req: IncomingMessage, res: ServerResponse) => void {
  return (req, res) => {
    let i = -1
    const next = (proceed: boolean) => {
      if (!proceed) {
        return
      }

      i++
      if (i >= middleware.length) {
        handler(req, res)
        return
      }

      middleware[i](client, req, res, next)
    }

    next(true)
  }
}

const defaultMiddleware: Middleware[] = [checkPost, parseJson]

export default function(
  connectOptions: ConnectOptions,
  middlewares?: Middleware[]
): {
  get: (req: IncomingMessage, res: ServerResponse) => void
  set: (req: IncomingMessage, res: ServerResponse) => void
  delete: (req: IncomingMessage, res: ServerResponse) => void
  updateSchema: (req: IncomingMessage, res: ServerResponse) => void
} {
  const middleware: Middleware[] = defaultMiddleware.concat(middlewares || [])

  const client = connect(connectOptions)

  return {
    get: applyMiddleware(
      client,
      middleware,
      (req: IncomingMessage, res: ServerResponse) => {
        // TODO: middleware
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
            return sendError(res, e)
          })
      }
    ),
    set: applyMiddleware(
      client,
      middleware,
      (req: IncomingMessage, res: ServerResponse) => {
        const body: any = (<any>req).body
        if (!body.$source) {
          body.$source = {
            $name: 'api',
            $overwrite: true
          }
        }

        // FIXME: this is some bullshit logic right here
        // that we use to check if something meaningful will change with this set
        // to reduce load on the cluster due to replication
        // NUKE THIS PLS
        const fakeGetBody: GetOptions = constructGuard(body)
        client
          .get(fakeGetBody)
          .then(result => {
            let needsChanges = true
            try {
              const r = !result.id ? false : noHasGuard(body, result)
              needsChanges = !r
            } catch (_e) {}

            if (!needsChanges) {
              console.log('HAHA, NOTHING ACTUALLY UPDATED')
              return Promise.resolve(result.id)
            }

            console.log('ACTUALLY SETTING')
            // only this needs to be the "entrypoint" to the promise chain... later at least
            return client.set(body)
          })
          .then(result => {
            if (!result) {
              console.error('Nothing was created')
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
            return sendError(res, e)
          })
      }
    ),
    delete: applyMiddleware(
      client,
      middleware,
      (req: IncomingMessage, res: ServerResponse) => {
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
            return sendError(res, e)
          })
      }
    ),
    updateSchema: applyMiddleware(
      client,
      middleware,
      (req: IncomingMessage, res: ServerResponse) => {
        const parsedUrl = url.parse(req.url, true)

        const dbName: string =
          <string>(parsedUrl.query && parsedUrl.query.dbName) || 'default'

        const body: any = (<any>req).body
        client
          .updateSchema(body)
          .then(() => {
            res.statusCode = 200
            res.setHeader('content-type', 'application/json')
            res.end(JSON.stringify(client.schemas[dbName]))
          })
          .catch(e => {
            console.error('Error setting schema', e)
            return sendError(res, e)
          })
      }
    )
  }
}
