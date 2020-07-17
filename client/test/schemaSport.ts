import test from 'ava'
import { connect } from '../src/index'
import { Schema, SchemaOptions, Fields, FieldSchema } from '../src/schema'
import { start } from '@saulx/selva-server'
import './assertions'
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
    createdAt: {
      type: 'timestamp'
      // search: { type: ['NUMERIC', 'SORTABLE'] } // do or not?
    },
    updatedAt: {
      type: 'timestamp'
      // search: { type: ['NUMERIC', 'SORTABLE'] } // do or not?
    },
    title: {
      type: 'text',
      search: { type: ['TEXT'] }
    }
  }

  const price: FieldSchema = {
    type: 'object',
    properties: types.reduce((properties, type) => {
      properties[type] = { type: 'int' }
      return properties
    }, {})
  }

  const contentFields: Fields = {
    ...defaultFields,
    price,
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
    article: {
      type: 'string'
    },
    // uuid: {
    //   type: 'object',
    //   properties: {
    //     sas: {
    //       type: 'string',
    //       search: { type: ['TAG'] }
    //     },
    //     dfb: {
    //       type: 'string',
    //       search: { type: ['TAG'] }
    //     },
    //     sams: {
    //       type: 'string',
    //       search: { type: ['TAG'] }
    //     },
    //     stripe: {
    //       type: 'string',
    //       search: { type: ['TAG'] }
    //     }
    //   }
    // },
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
    },
    allowGeo: {
      type: 'set',
      items: {
        type: 'string'
      }
    }
  }

  const startTime: FieldSchema = {
    type: 'timestamp',
    search: { type: ['NUMERIC'] }
  }

  const endTime: FieldSchema = {
    type: 'timestamp',
    search: { type: ['NUMERIC'] }
  }

  const gender: FieldSchema = {
    type: 'string'
  }

  const status: FieldSchema = {
    type: 'string',
    search: { type: ['TAG'] }
  }

  const contact: FieldSchema = {
    // maybe fixed props?
    type: 'json'
  }

  const videoFields: Fields = {
    ...contentFields,
    date: {
      type: 'timestamp',
      search: { type: ['NUMERIC', 'SORTABLE'] }
    },
    startTime,
    endTime,
    gender,
    status,
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
        ...contentFields
      }
    },
    types: {
      match: {
        prefix: 'ma',
        fields: {
          ...videoFields,
          highlights: {
            type: 'array',
            items: {
              type: 'object',
              properties: {
                value: { type: 'number' },
                uuid: { type: 'string' },
                description: { type: 'string' },
                type: { type: 'number' },
                durationMs: { type: 'number' },
                duration: { type: 'string' },
                startMs: { type: 'number' },
                start: { type: 'string' }
              }
            }
          }
        }
      },
      video: {
        prefix: 'vi',
        fields: {
          ...videoFields
        }
      },
      club: {
        prefix: 'cl',
        fields: {
          ...contentFields,
          cameras: {
            type: 'boolean'
          },
          discountCodes: {
            type: 'array',
            items: {
              type: 'object',
              properties: {
                code: {
                  type: 'string'
                },
                amount: {
                  type: 'number'
                }
              }
            }
          },
          contact
        }
      },
      team: {
        prefix: 'te',
        fields: {
          ...contentFields
        }
      },
      season: {
        prefix: 'se',
        fields: {
          ...contentFields,
          startTime,
          endTime
        }
      },
      league: {
        prefix: 'le',
        fields: {
          ...contentFields
        }
      },
      show: {
        prefix: 'sh',
        fields: {
          ...contentFields
        }
      },
      custom: {
        prefix: 'cu',
        fields: {
          ...videoFields
        }
      },
      sport: {
        prefix: 'sp',
        fields: {
          ...contentFields
        }
      },
      event: {
        prefix: 'ev',
        fields: {
          ...videoFields
        }
      },
      federation: {
        prefix: 'fe',
        fields: {
          ...contentFields
        }
      },
      product: {
        prefix: 'pr',
        fields: {
          ...defaultFields,
          value: {
            type: 'number'
          },
          price,
          startTime,
          endTime
        }
      },
      ad: {
        prefix: 'ad',
        fields: {
          ...contentFields,
          startTime,
          endTime,
          user: {
            type: 'string'
          },
          seller: {
            type: 'string'
          },
          thirdParty: {
            type: 'boolean'
          },
          status,
          paymentData: {
            type: 'json'
          },
          contact
        }
      },
      series: {
        prefix: 'sr',
        fields: {
          ...contentFields
        }
      },
      category: {
        prefix: 'ct',
        fields: {
          ...contentFields
        }
      },
      class: {
        prefix: 'cs',
        fields: {
          ...contentFields
        }
      },
      article: {
        prefix: 'ar',
        fields: {
          ...contentFields
        }
      }
    }
  }

  await client.updateSchema(schema)

  const { schema: schemaResult, searchIndexes } = await client.getSchema()

  server.destroy()

  t.true(true)
})
