const { connect } = require('@saulx/selva')
const { start } = require('@saulx/selva-server')
const fs = require('fs').promises
const os = require('os')
const path = require('path')
const _ = require('lodash')

const IGNORE_UNTIL = null

const REMAPPED_FIELDS = {
  streamStart: 'start',
  streamEnd: 'end'
}

function remapField(field) {
  if (REMAPPED_FIELDS[field]) {
    return REMAPPED_FIELDS[field]
  }

  return field
}

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
      search: { type: ['TEXT-LANGUAGE-SUG'] }
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

  const start = {
    type: 'timestamp',
    search: { type: ['NUMERIC', 'SORTABLE'] }
  }

  const end = {
    type: 'timestamp',
    search: { type: ['NUMERIC', 'SORTABLE'] }
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
    start,
    end,
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
        ...contentFields,
        aliases: {
          type: 'set',
          items: {
            type: 'string'
          }
        }
      }
    },
    types: {
      match: {
        prefix: 'ma',

        hierarchy: {
          team: {
            excludeAncestryWith: ['league']
          }
        },

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
      region: {
        prefix: 're',
        fields: {
          ...contentFields
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

        hierarchy: {
          team: {
            excludeAncestryWith: ['league']
          }
        },

        fields: {
          ...contentFields
        }
      },
      season: {
        prefix: 'se',
        fields: {
          ...contentFields,
          start,
          end
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
          start,
          end
        }
      },
      ad: {
        prefix: 'ad',
        fields: {
          ...contentFields,
          start,
          end,
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

  console.log(schema)

  await client.updateSchema(schema)
}

function constructSetProps(id, prefixToTypeMapping, typeSchema, item) {
  const props = {}
  for (const itemKey in item) {
    if (!item[itemKey] || item[itemKey] === '') {
      continue
    }

    if (itemKey === 'ancestors' || itemKey.endsWith(':from')) {
      // skip from keys for now
      continue
    }

    if (typeSchema.fields) {
      if (typeSchema.fields[remapField(itemKey)]) {
        const fieldType = typeSchema.fields[remapField(itemKey)].type
        switch (fieldType) {
          case 'object':
            if (!item[itemKey] || item[itemKey] === '') {
              continue
            }

            try {
              const value = JSON.parse(item[itemKey])
              if (itemKey === 'video') {
                /*
                  video: [{
                    type: 'vod',
                    mp4: 'haha'
                    m3u8: 'haha'
                  }, {
                    type: 'pano',
                    mp4: 'haha'
                    m3u8: 'haha'
                  }, {
                    {
                    type: 'live',
                    mp4: 'haha'
                    m3u8: 'haha'
                  }
                  }
                ]
                ===>
                video: {
                  vod: {
                    mp4: 
                    hls: 
                  },
                  pano: {
                    mp4: 
                    hls:
                  },
                  live: {
                    mp4:
                    hls: 
                  }
                }
                */
                if (Array.isArray(value)) {
                  const video = {}
                  let pass
                  value.forEach(({ type, mp4, m3u8 }) => {
                    if (type !== 'pano' && type !== 'live') {
                      type = 'vod'
                    }
                    if (!video[type]) {
                      video[type] = {}
                    }
                    if (mp4 && /^http/.test(mp4)) {
                      video[type].mp4 = encodeURI(mp4)
                      pass = true
                    }
                    if (m3u8 && /^http/.test(m3u8)) {
                      video[type].hls = encodeURI(m3u8)
                      pass = true
                    }
                  })
                  if (pass) {
                    props[remapField(itemKey)] = video
                  }
                }
              } else {
                const newSchema = {
                  type: 'object',
                  fields: typeSchema.fields[remapField(itemKey)].properties
                }
                props[remapField(itemKey)] = constructSetProps(
                  id,
                  prefixToTypeMapping,
                  newSchema,
                  value
                )
              }
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
          case 'text':
          case 'references':
          case 'array':
          case 'set':
          case 'json':
            if (
              (fieldType === 'array' || fieldType === 'set') &&
              typeSchema.fields[remapField(itemKey)].items.type === 'object'
            ) {
              const newSchema = {
                type: 'object',
                fields: typeSchema.fields[remapField(itemKey)].items.properties
              }

              const ary = JSON.parse(item[itemKey])
              if (!Array.isArray(ary)) {
                continue
              }

              props[remapField(itemKey)] = ary.map(x => {
                return constructSetProps(id, prefixToTypeMapping, newSchema, x)
              })
              continue
            }

            if (!item[itemKey] || item[itemKey] === '') {
              continue
            }

            if (item[itemKey] === '{}') {
              continue
            }

            const parsed = JSON.parse(item[itemKey])
            if (fieldType === 'references') {
              if (id === 'cujpQXzXZ') {
                props[remapField(itemKey)] = []
                continue
              }

              let relations = []
              for (const relation of parsed) {
                const prefix = relation.slice(0, 2)
                if (
                  prefixToTypeMapping[prefix] &&
                  /* exclude self references */
                  relation !== id &&
                  relation !== 'relnqzJe'
                ) {
                  // console.log('adding relation for', id, relation)
                  relations.push(relation)
                }
              }

              if (relations.length) {
                props[remapField(itemKey)] = relations
              }
            } else {
              props[remapField(itemKey)] = parsed
            }
            break
          case 'boolean':
            props[remapField(itemKey)] = !!Number(item[itemKey])
            break
          case 'int':
          case 'float':
          case 'number':
          case 'timestamp':
            if (item[itemKey] === '0') {
              continue
            }

            props[remapField(itemKey)] = Number(item[itemKey])
            break
          case 'url':
          case 'string':
            if (Array.isArray(item[itemKey])) {
              if (item[itemKey] === '{}' || item[itemKey][0] === '') {
                continue
              }

              props[remapField(itemKey)] = item[itemKey][0]
            } else {
              props[remapField(itemKey)] = item[itemKey]
            }
            break
          default:
            break
        }
      }
    }
  }

  return props
}

async function migrate() {
  // const srv = await start({ port: 6061 })
  const client = connect({ port: 6061 } /*, { loglevel: 'info' }*/)

  await makeSchema(client)

  const dump = JSON.parse(
    await fs.readFile(path.join(os.homedir(), 'Downloads', 'dump-last.json'))
  )

  const schema = await client.getSchema()

  const clean = data => {
    const blacklist = new Set()
    const checkBad = (item, blacklist) => {
      if (blacklist.has(item.id)) {
        return true
      }
      if (!Number(item.published)) {
        if (
          !item.parents ||
          !JSON.parse(item.parents).length ||
          /"Copy of /.test(item.title)
        ) {
          blacklist.add(item.id)
          return true
        }
        let keep
        for (let i in item) {
          if (/:from$/.test(i) && i !== 'published:from') {
            const [origin] = item[i].split('-')
            if (origin === 'set') keep = true
          }
        }
        if (!keep) {
          try {
            keep = JSON.parse(item.children).find(id => {
              const bad = checkBad(data[id], blacklist)
              return !bad
            })
          } catch (e) {}
        }
        if (!keep) {
          blacklist.add(item.id)
          return true
        }
      }
    }

    for (const id in data) {
      const item = data[id]
      if (
        !item.type ||
        item.type === 'location' ||
        item.type === 'sponsorship'
      ) {
        continue
      }
      if (checkBad(item, blacklist)) {
        delete data[id]
      }
    }

    for (const id in data) {
      const item = data[id]
      try {
        item.parents = JSON.stringify(
          JSON.parse(item.parents).filter(id => !blacklist.has(id))
        )
      } catch (e) {}
      try {
        item.children = JSON.stringify(
          JSON.parse(item.children).filter(id => !blacklist.has(id))
        )
      } catch (e) {}
    }
  }

  let ignore = IGNORE_UNTIL ? true : false

  for (let db of dump) {
    clean(db)
    // console.log(db.vi0dMzRO)
    // continue
    // db.root.url = db.root.url.filter(val => val)
    // db = { rez5lmBya: db.rez5lmBya, uuid: db.uuid }
    const keys = Object.keys(db)
    const batches = _.chunk(keys, 3000)
    // const batches = [['root', 'rez5lmBya']] //'rez5lmBya']]
    for (const batch of batches) {
      // console.log('batch', batch)
      let promises = []
      for (const key of batch) {
        // ignore deprecated
        if (key === 'relnqzJe') {
          continue
        }

        if (ignore) {
          if (key === IGNORE_UNTIL) {
            ignore = false
          } else {
            continue
          }
        }

        if (key === undefined || key === 'undefined') {
          continue
        }

        const item = db[key]
        if (!item.type) {
          continue
        }

        // console.log('processing key', key, 'type', item.type, item)

        const typeSchema =
          key === 'root'
            ? schema.schema.rootType
            : schema.schema.types[item.type]

        if (!typeSchema) {
          console.log('No type schema found for', item.type)
          continue
        }

        const props = constructSetProps(
          key,
          schema.schema.prefixToTypeMapping,
          typeSchema,
          item
        )

        const initialPayload = {
          $id: key,
          ...props
        }

        const newPayload = await client.conformToSchema(initialPayload)

        if (!newPayload) {
          console.error('no payload for key', props, key, item)
          process.exit(1)
        }

        const aliases = []
        if (item.uuid) {
          const uuids = item.uuid.split(',')
          for (const uuid of uuids) {
            let uuidSource = db.uuids[uuid]
            if (!uuidSource) {
              uuidSource = 'sas'
            }

            aliases.push(`${uuidSource}-${uuid}`)
          }
        }

        if (item.url) {
          try {
            const obj = JSON.parse(item.url)
            if (typeof obj == 'object') {
              for (const key in obj) {
                if (obj[key]) {
                  aliases.push(obj[key])
                }
              }
            }
          } catch (_e) {
            aliases.push(item.url)
          }
        }

        if (aliases.length) {
          newPayload.aliases = (newPayload.aliases || []).concat(aliases)
        }

        // delete newPayload.title
        // console.log('inserting', newPayload)
        promises.push(client.set(newPayload))

        await new Promise((resolve, _reject) => {
          setTimeout(resolve, 1)
        })
      }

      await Promise.all(promises)
    }
  }

  await client.destroy()
  // await srv.destroy()
}

migrate()
  .then(() => {
    process.exit(0)
  })
  .catch(e => {
    console.error(e)
    process.exit(1)
  })
