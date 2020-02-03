import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'
import { FieldType, Fields, Schema } from '../src/schema'
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

// maybe add this somewhere
// same for image, video etc

const mangleResults = (correctSchema: Schema, schemaResult: Schema) => {
  if (!correctSchema.sha) {
    delete schemaResult.sha
  }

  for (const type in schemaResult.types) {
    if (!correctSchema.types[type].prefix) {
      delete schemaResult.types[type].prefix
    }
  }
  delete schemaResult.idSeedCounter
  delete schemaResult.prefixToTypeMapping
}

test.skip('schemas - basic', async t => {
  let current = { port: 6066 }

  const server = await start({
    port: 6066,
    developmentLogging: true,
    loglevel: 'info'
  })
  const client = connect({ port: 6066 })

  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })

  const defaultFields: Fields = {
    id: {
      type: 'id'
    },
    type: {
      type: 'type',
      search: {
        index: 'default',
        type: ['TAG']
      }
    },
    title: {
      type: 'text'
    },
    parents: {
      type: 'references'
    },
    children: {
      type: 'references'
    },
    ancestors: {
      type: 'references'
    },
    descendants: {
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

  const { schema: schemaResult, searchIndexes } = await client.getSchema()

  mangleResults(schema, schemaResult)
  t.deepEqual(schemaResult, schema, 'correct schema')

  t.deepEqualIgnoreOrder(
    Object.keys(schema.types),
    ['league', 'person', 'video', 'vehicle', 'family', 'match'],
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

  const newResult = (await client.getSchema()).schema
  mangleResults(schema, newResult)
  console.log(`newResult`, newResult)
  t.deepEqual(newResult, schema, 'correct schema after setting the same')

  // drop search index in this case (NOT SUPPORTED YET!)
  // throws for now
  const e = await t.throwsAsync(
    client.updateSchema({
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
  )

  t.true(
    e.stack.includes(
      'Can not change existing search types for flurpy.snurkels in type match, changing from ["TAG"] to ["TEXT"]. This will be supported in the future.'
    )
  )

  const info2 = await client.redis.ftInfo('default')
  const fields2 = info2[info2.indexOf('fields') + 1]

  // does not drop and create a new one for now...
  t.deepEqual(
    fields2,
    [
      ['flurpy.snurkels', 'type', 'TAG', 'SEPARATOR', ','],
      ['type', 'type', 'TAG', 'SEPARATOR', ',']
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

  t.deepEqualIgnoreOrder(
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

  t.is(await client.getTypeFromId('maflurpy'), 'match')

  await client.updateSchema({
    types: {
      match: {
        fields: {
          flurpbird: { type: 'digest' },
          date: { type: 'timestamp' }
        }
      }
    }
  })

  await client.set({
    type: 'match',
    video: {
      mp4: 'https://flappie.com/clowns.mp4'
    },
    flurpbird: 'hello',
    date: 100000,
    title: {
      en: 'best match'
    },
    children: [
      {
        type: 'person',
        parents: { $add: 'root' }
      }
    ],
    flapperdrol: { smurky: true }
  })

  // add some tests for it

  server.destroy()
})

test('schemas - search indexes', async t => {
  const server = await start({
    port: 6091,
    developmentLogging: true,
    loglevel: 'info'
  })
  const client = connect({ port: 6091 })

  await client.updateSchema({
    languages: ['nl', 'en'],
    types: {
      league: {
        fields: {
          value: { type: 'number', search: { type: ['NUMERIC'] } }
        }
      },
      person: {
        fields: {
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      }
    }
  })

  t.deepEqual(
    (await client.getSchema()).searchIndexes,
    {
      default: {
        type: ['TAG'],
        ancestors: ['TAG'],
        value: ['NUMERIC', 'SORTABLE'],
        parents: ['TAG'],
        children: ['TAG'],
        id: ['TAG']
      }
    },
    'Sort will be added even if type is allready defined'
  )

  await client.updateSchema({
    types: {
      flurp: {
        fields: {
          x: { type: 'string', search: { type: ['TEXT'] } }
        }
      },
      flap: {
        fields: {
          x: { type: 'string', search: { type: ['NUMERIC'] } }
        }
      }
    }
  })

  t.deepEqual(
    (await client.getSchema()).searchIndexes,
    {
      default: {
        type: ['TAG'],
        ancestors: ['TAG'],
        value: ['NUMERIC', 'SORTABLE'],
        parents: ['TAG'],
        children: ['TAG'],
        id: ['TAG'],
        x: ['NUMERIC']
      }
    },
    'Sort will be added even if type is allready defined'
  )

  await client.updateSchema({
    types: {
      flurp: {
        fields: {
          flarp: { type: 'string', search: true }
        }
      }
    }
  })

  t.deepEqual(
    (await client.getSchema()).searchIndexes,
    {
      default: {
        value: ['NUMERIC', 'SORTABLE'],
        x: ['NUMERIC'],
        type: ['TAG'],
        id: ['TAG'],
        parents: ['TAG'],
        flarp: ['TEXT'],
        children: ['TAG'],
        ancestors: ['TAG']
      }
    },
    'Search:true auto casts'
  )

  await client.updateSchema({
    types: {
      flurp: {
        fields: {
          flux: { type: 'string' }
        }
      }
    }
  })

  /*
    supported languages
    "arabic", "danish", "dutch", "english", "finnish", "french", "german", "hungarian", "italian", "norwegian", "portuguese", "romanian", "russian", "spanish", "swedish", "tamil", "turkish" "chinese"
    */
  await client.updateSchema({
    types: {
      flurp: {
        fields: {
          title: { type: 'text', search: { type: ['TEXT-LANGUAGE'] } }
        }
      }
    }
  })

  t.deepEqual(
    (await client.getSchema()).searchIndexes,
    {
      default: {
        ancestors: ['TAG'],
        parents: ['TAG'],
        x: ['NUMERIC'],
        title: ['TEXT-LANGUAGE'],
        type: ['TAG'],
        value: ['NUMERIC', 'SORTABLE'],
        flarp: ['TEXT'],
        children: ['TAG'],
        id: ['TAG']
      }
    },
    'Includes text-language'
  )

  const { searchIndexes } = await client.getSchema()

  await client.updateSchema({
    languages: ['nl', 'en', 'de']
  })

  const info = await client.redis.ftInfo('default')
  const fields = info[info.indexOf('fields') + 1]

  t.deepEqual(
    fields,
    [
      ['type', 'type', 'TAG', 'SEPARATOR', ','],
      ['ancestors', 'type', 'TAG', 'SEPARATOR', ','],
      ['value', 'type', 'NUMERIC', 'SORTABLE'],
      ['parents', 'type', 'TAG', 'SEPARATOR', ','],
      ['children', 'type', 'TAG', 'SEPARATOR', ','],
      ['id', 'type', 'TAG', 'SEPARATOR', ','],
      ['x', 'type', 'NUMERIC'],
      ['flarp', 'type', 'TEXT', 'WEIGHT', '1'],
      ['title.nl', 'type', 'TEXT', 'WEIGHT', '1'],
      ['title.en', 'type', 'TEXT', 'WEIGHT', '1'],
      ['title.de', 'type', 'TEXT', 'WEIGHT', '1']
    ],
    'Index includes language fields'
  )

  // then add geo case

  await wait()

  server.destroy()

  t.true(true)
})
