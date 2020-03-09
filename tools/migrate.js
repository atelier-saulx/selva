const { connect } = require('@saulx/selva')
const { start } = require('@saulx/selva-server')
const fs = require('fs').promises
const os = require('os')
const path = require('path')

async function makeSchema(client) {
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

  const defaultFields = {
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

  const price = {
    type: 'object',
    properties: types.reduce((properties, type) => {
      properties[type] = { type: 'int' }
      return properties
    }, {})
  }

  const contentFields = {
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

  const startTime = {
    type: 'timestamp',
    search: { type: ['NUMERIC'] }
  }

  const endTime = {
    type: 'timestamp',
    search: { type: ['NUMERIC'] }
  }

  const gender = {
    type: 'string'
  }

  const status = {
    type: 'string',
    search: { type: ['TAG'] }
  }

  const contact = {
    // maybe fixed props?
    type: 'json'
  }

  const videoFields = {
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

  const schema = {
    languages: ['en', 'de', 'fr', 'nl', 'it'],
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
}

async function migrate() {
  const srv = await start({ port: 6061 })
  const client = connect({ port: 6061 }, { loglevel: 'info' })

  await makeSchema(client)

  const dump = JSON.parse(
    await fs.readFile(path.join(os.homedir(), 'Downloads', 'dump-last.json'))
  )

  const schema = await client.getSchema()
  for (const db of dump) {
    for (const key in db) {
      const item = db[key]
      if (!item.type) {
        continue
      }

      console.log('processing key', key, 'type', item.type)
      const props = {}

      const typeSchema =
        key === 'root' ? schema.schema.rootType : schema.schema.types[item.type]

      if (!typeSchema) {
        console.log('No type schema found for', item.type)
        continue
      }

      for (const itemKey in item) {
        if (itemKey.endsWith(':from')) {
          // skip from keys for now
          continue
        }

        if (typeSchema.fields) {
          if (typeSchema.fields[itemKey]) {
            const fieldType = typeSchema.fields[itemKey].type
            console.log('itemkey', itemKey, fieldType)
            switch (fieldType) {
              case 'array':
              case 'text':
              case 'object':
              case 'json':
                if (!item[itemKey] || item[itemKey] === '') {
                  continue
                }

                try {
                  props[itemKey] = JSON.parse(item[itemKey])
                } catch (e) {
                  console.error(
                    'Error processing json field value for',
                    itemKey,
                    item,
                    e
                  )
                  process.exit(1)
                }
                break
              case 'boolean':
                props[itemKey] = !!Number(item[itemKey])
                break
              case 'int':
              case 'float':
              case 'number':
              case 'timestamp':
                props[itemKey] = Number(item[itemKey])
                break
              case 'string':
                props[itemKey] = item[itemKey]
              default:
                break
            }
          }
        } else if (itemKey === 'parents' || itemKey === 'children') {
          props[itemKey] = JSON.parse(item[itemKey])
        }
      }

      console.log(props)

      const initialPayload = {
        $id: key,
        ...props
      }

      const newPayload = await client.conformToSchema(initialPayload)
      console.log('inserting', newPayload)
      await client.set(newPayload)
    }
  }

  await client.destroy()
  // await srv.destroy()
}

migrate()
  .then(() => {
    process.exit(1)
  })
  .catch(e => {
    console.error(e)
    process.exit(1)
  })
