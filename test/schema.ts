import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'
import { FieldType, Fields, Schema } from '../src/schema'
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

  const schema: Schema = {
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
          ...defaultFields,
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
          flurpy: {
            type: 'object',
            properties: {
              snurkels: {
                type: 'string',
                search: { type: ['TAG'] }
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
                type: 'url',
                search: { index: 'hls', type: ['TEXT'] }
              },
              pano: {
                type: 'url'
              },
              overlays: {
                type: 'array',
                items: {
                  type: 'json', // needs to be json!
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

  await client.updateSchema(schema)

  const {
    types,
    schema: schemaResult,
    searchIndexes
  } = await client.getSchema()

  t.deepEqual(schemaResult, schema, 'correct schema')

  t.deepEqualIgnoreOrder(
    Object.keys(types),
    ['idSize', 'league', 'person', 'video', 'vehicle', 'family', 'match'],
    'correct type map'
  )

  t.deepEqual(
    searchIndexes,
    {
      default: { type: ['TAG'], 'flurpy.snurkels': ['TAG'] },
      hls: { 'video.hls': ['TEXT'] }
    },
    'searchIndexes are equal'
  )

  t.true(
    (await client.redis.keys('*')).includes('idx:default'),
    'made redis-search index for default'
  )

  t.true(
    (await client.redis.keys('*')).includes('idx:hls'),
    'made redis-search index for hls'
  )

  await client.updateSchema({
    types: {
      match: {
        fields: {
          ...defaultFields
        }
      }
    }
  })

  t.deepEqual(
    (await client.getSchema()).schema,
    schema,
    'correct schema after setting the same'
  )

  // drop search index in this case (NOT SUPPORTED YET!)
  await client.updateSchema({
    types: {
      match: {
        fields: {
          flurpy: {
            type: 'object',
            properties: {
              snurkels: {
                type: 'string',
                search: { type: ['TEXT'] }
              }
            }
          }
        }
      }
    }
  })

  const info2 = await client.redis.ftInfo('default')
  const fields2 = info2[info2.indexOf('fields') + 1]

  // does not drop and create a new one for now...
  t.deepEqual(
    fields2,
    [
      ['type', 'type', 'TAG', 'SEPARATOR', ','],
      ['flurpy.snurkels', 'type', 'TAG', 'SEPARATOR', ',']
    ],
    'change fields in the index - does not drop index yet so stays the same!'
  )

  await client.updateSchema({
    types: {
      match: {
        fields: {
          flurpy: {
            type: 'object',
            properties: {
              snurpie: {
                type: 'string',
                search: { type: ['TEXT'] }
              }
            }
          }
        }
      }
    }
  })

  const info = await client.redis.ftInfo('default')
  const fields = info[info.indexOf('fields') + 1]

  t.deepEqual(
    fields,
    [
      ['type', 'type', 'TAG', 'SEPARATOR', ','],
      ['flurpy.snurkels', 'type', 'TAG', 'SEPARATOR', ','],
      ['flurpy.snurpie', 'type', 'TEXT', 'WEIGHT', '1']
    ],
    'add fields to the index'
  )

  t.deepEqual(
    (await client.getSchema()).schema.types.match.fields.flurpy,
    {
      type: 'object',
      properties: {
        snurkels: {
          type: 'string',
          search: {
            type: ['TAG']
          }
        },
        snurpie: {
          type: 'string',
          search: {
            type: ['TEXT']
          }
        }
      }
    },
    'added field to object schema'
  )

  // sends hierarchy update
  await client.updateSchema({
    types: {
      match: {
        hierarchy: {
          team: { excludeAncestryWith: ['league'] },
          video: false,
          $default: { excludeAncestryWith: ['vehicle'] }
        }
      }
    }
  })

  t.deepEqual(
    (await client.getSchema()).schema.types.match.hierarchy,
    {
      team: { excludeAncestryWith: ['league'] },
      video: false,
      $default: { excludeAncestryWith: ['vehicle'] }
    },
    'updated hierarchy schema'
  )

  // add some tests for it

  server.destroy()
})
