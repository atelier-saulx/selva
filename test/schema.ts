import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import { wait } from './assertions'

// id map
// fields
// hierarchy
// search index

/*
// TYPES
// float
// int
// number
// json
// array (stored as json)
// references (set of ids) // children or parents
// set
// string
// object
// text (with languages)
// id // reasonable length etc
// digest (password etc) //crypto hash
// timestamp (ms) //accepts 'now' as well
// url
// email
// phone 
// geo
// type
*/

const schema = {
  languages: ['nl', 'en'],
  type: {
    match: {
      hierarchy: {
        team: { excludeAncestryWith: ['league'] },
        video: false,
        person: { includeAncestryWith: ['family'] },
        $default: { excludeAncestryWith: ['vehicle'] }
      },
      fields: {
        id: {
          type: 'id'
        },
        type: {
          search: { index: 'default', type: ['TAG'] }, // hard to update :(
          type: 'type'
        },
        title: {
          type: 'text'
        },
        children: {
          type: 'references'
        },
        parents: {
          type: 'references'
        },
        smurky: {
          type: 'set',
          items: {
            type: 'object',
            properties: {
              interval: {
                type: 'array',
                items: {
                  type: 'timestamp'
                }
              },
              url: { type: 'url' }
            }
          }
        },
        flapperdrol: {
          type: 'json'
        },
        video: {
          type: 'object',
          properties: {
            mp4: {
              type: 'url'
            },
            hls: {
              type: 'url'
            },
            pano: {
              type: 'url'
            },
            overlays: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  interval: {
                    type: 'array',
                    items: {
                      type: 'timestamp'
                    }
                  },
                  url: { type: 'url' }
                }
              }
            }
          }
        }
      }
    }
  }
}

test('schemas - basic', async t => {
  let current = { port: 6066 }

  const server = await start({ port: 6066, modules: ['redisearch'] })
  const client = await connect({ port: 6066 })

  await client.updateSchema(schema)
  server.destroy()
})
