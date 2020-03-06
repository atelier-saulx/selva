import test from 'ava'
import { connect, Fields, Schema, SchemaOptions } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

const mangleResults = (
  correctSchema: Schema | SchemaOptions,
  schemaResult: Schema
) => {
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

test.serial.only('schemas - sport', async t => {
  const port = await getPort()
  const server = await start({
    port
  })
  const client = connect({ port })

  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })

  const types = [
    'ad',
    'article',
    'category',
    'class',
    'club',
    'custom',
    'event',
    'federation',
    'league',
    'match',
    'product',
    'region',
    'season',
    'series',
    'show',
    'sport',
    'team',
    'video'
  ]

  const defaultFields: Fields = {
    title: {
      type: 'text',
      search: { type: ['TEXT'] }
    },
    description: {
      type: 'text'
    },
    published: {
      type: 'boolean',
      search: { type: ['TAG'] }
    },
    rating: {
      type: 'int',
      search: { type: ['NUMERIC', 'SORTABLE'] }
    },
    overlay: {
      type: 'string'
    },
    price: {
      type: 'object',
      properties: types.reduce((properties, type) => {
        properties[type] = { type: 'int' }
        return properties
      }, {})
    },
    uuid: {
      type: 'object',
      properties: {
        sas: {
          type: 'string',
          search: { type: ['TAG'] }
        },
        dfb: {
          type: 'string',
          search: { type: ['TAG'] }
        },
        sams: {
          type: 'string',
          search: { type: ['TAG'] }
        }
      }
    },
    image: {
      type: 'object',
      properties: {
        logo: {
          type: 'url'
        },
        cover: {
          type: 'url'
        },
        thumb: {
          type: 'url'
        }
      }
    }
  }

  const videoFields: Fields = {
    date: {
      type: 'timestamp',
      search: { type: ['NUMERIC', 'SORTABLE'] }
    },
    start: {
      type: 'timestamp',
      search: { type: ['NUMERIC'] }
    },
    end: {
      type: 'timestamp',
      search: { type: ['NUMERIC'] }
    },
    status: {
      type: 'string',
      search: { type: ['TAG'] }
    },
    geo: {
      type: 'set',
      items: {
        type: 'string'
      }
    },
    video: {
      type: 'object',
      properties: {
        vod: {
          type: 'object',
          properties: {
            mp4: {
              type: 'url'
            },
            hls: {
              type: 'url'
            }
          }
        },
        pano: {
          type: 'object',
          properties: {
            mp4: {
              type: 'url'
            },
            hls: {
              type: 'url'
            }
          }
        },
        live: {
          type: 'object',
          properties: {
            mp4: {
              type: 'url'
            },
            hls: {
              type: 'url'
            }
          }
        }
      }
    }
  }

  const schema: SchemaOptions = {
    languages: ['nl', 'en'],
    rootType: {
      fields: {
        ...defaultFields
      }
    },
    types: {
      match: {
        fields: {
          ...defaultFields,
          ...videoFields,
          highlights: {
            type: 'json'
          },
          gender: {
            type: 'string'
          }
        }
      },
      video: {
        fields: {
          ...defaultFields,
          ...videoFields
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
      }
      // match: {
      //   prefix: 'ma',
      //   hierarchy: {
      //     team: { excludeAncestryWith: ['league'] },
      //     video: false,
      //     person: { includeAncestryWith: ['family'] },
      //     $default: { excludeAncestryWith: ['vehicle'] }
      //   },
      //   fields: {
      //     ...defaultFields,
      //     smurky: {
      //       meta: {
      //         yesh: 'a meta value',
      //         data: ['in an array']
      //       },
      //       type: 'set',
      //       items: {
      //         type: 'object', // stored as json in this case (scince you start with a set)
      //         properties: {
      //           interval: {
      //             type: 'array',
      //             items: {
      //               type: 'timestamp'
      //             }
      //           },
      //           url: { type: 'url' }
      //         }
      //       }
      //     },
      //     flurpy: {
      //       type: 'object',
      //       properties: {
      //         snurkels: {
      //           type: 'string',
      //           search: { type: ['TAG'] }
      //         }
      //       }
      //     },
      //     flapperdrol: {
      //       type: 'json'
      //     },
      //     video: {
      //       type: 'object',
      //       properties: {
      //         mp4: {
      //           type: 'url'
      //         },
      //         hls: {
      //           type: 'url',
      //           search: { index: 'hls', type: ['TEXT'] }
      //         },
      //         pano: {
      //           type: 'url'
      //         },
      //         overlays: {
      //           type: 'array',
      //           items: {
      //             type: 'json', // needs to be json!
      //             properties: {
      //               interval: {
      //                 type: 'array',
      //                 items: {
      //                   type: 'timestamp'
      //                 }
      //               },
      //               url: { type: 'url' }
      //             }
      //           }
      //         }
      //       }
      //     }
      //   }
      // }
    }
  }

  await client.updateSchema(schema)

  const { schema: schemaResult, searchIndexes } = await client.getSchema()

  // make sure meta is accessible
  t.deepEqual(schemaResult.types.match.fields.smurky.meta, {
    yesh: 'a meta value',
    data: ['in an array']
  })

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
      default: { type: ['TAG'], 'flurpy.snurkels': ['TAG'], name: ['TAG'] },
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
          }
        }
      }
    }
  })

  let newResult = (await client.getSchema()).schema

  mangleResults(schema, newResult)
  t.deepEqual(
    newResult,
    schema,
    'correct schema after setting the same without meta'
  )

  await client.updateSchema({
    types: {
      match: {
        fields: {
          smurky: {
            meta: 'overriden with string',
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
          }
        }
      }
    }
  })

  newResult = (await client.getSchema()).schema
  t.deepEqual(
    newResult.types.match.fields.smurky.meta,
    'overriden with string',
    'can overwrite meta'
  )

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
      ['type', 'type', 'TAG', 'SEPARATOR', ','],
      ['name', 'type', 'TAG', 'SEPARATOR', ','],
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
      ['name', 'type', 'TAG', 'SEPARATOR', ','],
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

  await client.set({
    $id: 'root',
    value: 9001
  })

  // add some tests for it

  server.destroy()
})

test.serial('schemas - search indexes', async t => {
  const port = await getPort()
  const server = await start({
    port
  })
  const client = connect({ port })

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
        children: ['TAG']
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
        children: ['TAG']
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
      ['value', 'type', 'NUMERIC', 'SORTABLE'],
      ['ancestors', 'type', 'TAG', 'SEPARATOR', ','],
      ['children', 'type', 'TAG', 'SEPARATOR', ','],
      ['parents', 'type', 'TAG', 'SEPARATOR', ','],
      ['x', 'type', 'NUMERIC'],
      ['flarp', 'type', 'TEXT', 'WEIGHT', '1'],
      ['title.nl', 'type', 'TEXT', 'WEIGHT', '1'],
      ['title.en', 'type', 'TEXT', 'WEIGHT', '1'],
      ['title.de', 'type', 'TEXT', 'WEIGHT', '1']
    ],
    'Index includes language fields'
  )

  const id = await client.set({
    type: 'flurp',
    title: { de: 'Gutten morgen' }
  })

  console.log(await client.get({ $id: id, title: true }))

  await wait()

  server.destroy()

  t.true(true)
})
