import test from 'ava'
import { connect } from '../src/index'
import { SchemaOptions, Fields, FieldSchema } from '../src/schema'
import { start } from '@saulx/selva-server'
import './assertions'
import getPort from 'get-port'

test.serial('schemas - sport', async (t) => {
  const port = await getPort()
  const server = await start({
    port,
  })
  const client = connect({ port })

  await new Promise((resolve) => {
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
    'video',
  ]

  const defaultFields: Fields = {
    createdAt: {
      type: 'timestamp',
    },
    updatedAt: {
      type: 'timestamp',
    },
    title: {
      type: 'text',
    },
  }

  const price: FieldSchema = {
    type: 'object',
    properties: types.reduce((properties, type) => {
      properties[type] = { type: 'int' }
      return properties
    }, {}),
  }

  const contentFields: Fields = {
    ...defaultFields,
    price,
    description: {
      type: 'text',
    },
    published: {
      type: 'boolean',
    },
    rating: {
      type: 'int',
    },
    overlay: {
      type: 'string',
    },
    article: {
      type: 'string',
    },
    image: {
      type: 'object',
      properties: {
        logo: {
          type: 'url',
        },
        cover: {
          type: 'url',
        },
        thumb: {
          type: 'url',
        },
      },
    },
    allowGeo: {
      type: 'set',
      items: {
        type: 'string',
      },
    },
  }

  const startTime: FieldSchema = {
    type: 'timestamp',
  }

  const endTime: FieldSchema = {
    type: 'timestamp',
  }

  const gender: FieldSchema = {
    type: 'string',
  }

  const status: FieldSchema = {
    type: 'string',
  }

  const contact: FieldSchema = {
    // maybe fixed props?
    type: 'json',
  }

  const videoFields: Fields = {
    ...contentFields,
    date: {
      type: 'timestamp',
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
              type: 'url',
            },
            hls: {
              type: 'url',
            },
          },
        },
        pano: {
          type: 'object',
          properties: {
            mp4: {
              type: 'url',
            },
            hls: {
              type: 'url',
            },
          },
        },
        live: {
          type: 'object',
          properties: {
            mp4: {
              type: 'url',
            },
            hls: {
              type: 'url',
            },
          },
        },
      },
    },
  }

  const schema: SchemaOptions = {
    languages: ['nl', 'en'],
    rootType: {
      fields: {
        ...contentFields,
      },
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
                start: { type: 'string' },
              },
            },
          },
        },
      },
      video: {
        prefix: 'vi',
        fields: {
          ...videoFields,
        },
      },
      club: {
        prefix: 'cl',
        fields: {
          ...contentFields,
          cameras: {
            type: 'boolean',
          },
          discountCodes: {
            type: 'array',
            items: {
              type: 'object',
              properties: {
                code: {
                  type: 'string',
                },
                amount: {
                  type: 'number',
                },
              },
            },
          },
          contact,
        },
      },
      team: {
        prefix: 'te',
        fields: {
          ...contentFields,
        },
      },
      season: {
        prefix: 'se',
        fields: {
          ...contentFields,
          startTime,
          endTime,
        },
      },
      league: {
        prefix: 'le',
        fields: {
          ...contentFields,
        },
      },
      show: {
        prefix: 'sh',
        fields: {
          ...contentFields,
        },
      },
      custom: {
        prefix: 'cu',
        fields: {
          ...videoFields,
        },
      },
      sport: {
        prefix: 'sp',
        fields: {
          ...contentFields,
        },
      },
      event: {
        prefix: 'ev',
        fields: {
          ...videoFields,
        },
      },
      federation: {
        prefix: 'fe',
        fields: {
          ...contentFields,
        },
      },
      product: {
        prefix: 'pr',
        fields: {
          ...defaultFields,
          value: {
            type: 'number',
          },
          price,
          startTime,
          endTime,
        },
      },
      ad: {
        prefix: 'ad',
        fields: {
          ...contentFields,
          startTime,
          endTime,
          user: {
            type: 'string',
          },
          seller: {
            type: 'string',
          },
          thirdParty: {
            type: 'boolean',
          },
          status,
          paymentData: {
            type: 'json',
          },
          contact,
        },
      },
      series: {
        prefix: 'sr',
        fields: {
          ...contentFields,
        },
      },
      category: {
        prefix: 'ct',
        fields: {
          ...contentFields,
        },
      },
      class: {
        prefix: 'cs',
        fields: {
          ...contentFields,
        },
      },
      article: {
        prefix: 'ar',
        fields: {
          ...contentFields,
        },
      },
    },
  }

  await client.updateSchema(schema)
  await client.getSchema()
  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()
})
