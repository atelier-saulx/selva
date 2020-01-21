import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
// import { wait } from './assertions'
import { FieldType, Fields } from '../src/schema'
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

// maybe add this somewhere
// same for image, video etc

test('schemas - basic', async t => {
  let current = { port: 6066 }

  const server = await start({ port: 6066, modules: ['redisearch'] })
  const client = await connect({ port: 6066 })

  const defaultFields: Fields = {
    id: {
      type: 'id'
    },
    type: {
      search: { index: 'default', type: ['TAG'] },
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
    }
  }

  await client.updateSchema({
    languages: ['nl', 'en'],
    types: {
      league: {
        fields: {
          ...defaultFields
        }
      },
      person: {
        fields: {
          ...defaultFields
        }
      },
      video: {
        fields: {
          ...defaultFields
        }
      },
      vehicle: {
        fields: {
          ...defaultFields
        }
      },
      family: {
        fields: {
          ...defaultFields
        }
      },
      match: {
        prefix: 'ma',
        hierarchy: {
          team: { excludeAncestryWith: ['league'] },
          video: false,
          person: { includeAncestryWith: ['family'] },
          $default: { excludeAncestryWith: ['vehicle'] }
        },
        fields: {
          smurky: {
            type: 'set',
            items: {
              type: 'object', // stored as json in this case (scince you start with a set)
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
  })

  console.log('xxxx---------------')
  await client.updateSchema({
    types: {
      flap: {
        fields: {
          ...defaultFields
        }
      }
    }
  })
  server.destroy()
})
