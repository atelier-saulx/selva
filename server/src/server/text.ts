import * as http from 'http'
import * as _url from 'url'
import { json } from 'body-parser'

import Flexsearch, { Index } from 'flexsearch'

Flexsearch.registerMatcher({
  ä: 'a',
  ö: 'o',
  ü: 'u',
})

const searchIndices: Record<string, Index<string>> = {}

const jsonParser = json({ limit: '50mb' })

type RequestBody = {
  // shitty typings for Flexsearch, this can actually be a string just fine
  $id?: any
  $field: string
  $searchString: string
  $language: string
}

export class TextServer {
  private srv: http.Server

  start(opts: { port?: number }) {
    this.srv = http.createServer(
      (req: http.IncomingMessage, res: http.ServerResponse) => {
        const url = _url.parse(req.url)
        const { pathname } = url

        jsonParser(req, res, (err) => {
          if (err) {
            console.error('Error parsing request body', err)
            res.statusCode = 400
            res.end('Bad request')
            return
          }

          const body: RequestBody = (<any>req).body
          const { $field, $language, $searchString } = body

          const idxKey = $field + '.' + $language
          let idx = searchIndices[idxKey]

          if (pathname === '/set') {
            if (!idx) {
              searchIndices[idxKey] = idx = Flexsearch.create()
            }

            idx.add(body.$id, $searchString)

            res.statusCode = 200
            res.end('OK')
          } else if (pathname === 'delete') {
            if (!idx) {
              console.error(`No index found for ${idxKey}`)
              res.statusCode = 200
              res.end('OK')
              return
            }

            idx.remove(body.$id)

            if (idx.length === 0) {
              idx.destroy()
              searchIndices[idxKey] = null
            }

            res.statusCode = 200
            res.end('OK')
          } else if (pathname === '/get') {
            if (!idx) {
              console.error(`No index found for ${idxKey}`)
              res.statusCode = 400
              res.end('Bad request')
              return
            }

            idx.search({ query: $searchString }, (results) => {
              if (!results) {
                results = []
              }

              res.statusCode = 200
              res.setHeader('content-type', 'application/json')
              res.end(JSON.stringify(results))
            })
          }
        })
      }
    )

    this.srv.listen(opts.port)
  }

  stop() {
    this.srv.close()
  }
}
