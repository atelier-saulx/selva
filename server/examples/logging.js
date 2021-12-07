const start = require('../dist/').start
const redis = require('redis')
const { connect } = require('@saulx/selva')

const defaultFields = {
  children: { type: 'references' },
  descendants: { type: 'references' },
  id: { type: 'id' },
  type: { type: 'type' },
}

const schema = {
  languages: ['nl', 'en'],
  rootType: {
    fields: {
      children: { type: 'references' },
      descendants: { type: 'references' },
      id: { type: 'id' },
      type: { type: 'type' },
      value: { type: 'number' },
    },
  },
  types: {
    league: {
      prefix: 'le',
      fields: {
        ...defaultFields,
      },
    },
    person: {
      prefix: 'pe',
      fields: {
        ...defaultFields,
      },
    },
    video: {
      prefix: 'vi',
      fields: {
        ...defaultFields,
      },
    },
    vehicle: {
      prefix: 've',
      fields: {
        ...defaultFields,
      },
    },
    family: {
      prefix: 'fa',
      fields: {
        ...defaultFields,
      },
    },
    team: {
      prefix: 'te',
      fields: {
        ...defaultFields,
      },
    },
    match: {
      prefix: 'ma',
      fields: {
        ...defaultFields,
        homeTeam: { type: 'reference' },
        awayTeam: { type: 'reference' },
        smurky: {
          meta: {
            yesh: 'a meta value',
            data: ['in an array'],
          },
          type: 'set',
          items: {
            type: 'object', // stored as json in this case (scince you start with a set)
            properties: {
              interval: {
                type: 'array',
                items: {
                  type: 'timestamp',
                },
              },
              url: { type: 'url' },
            },
          },
        },
        flurpy: {
          type: 'object',
          properties: {
            snurkels: {
              type: 'string',
              search: { type: ['TAG'] },
            },
          },
        },
        flapperdrol: {
          type: 'json',
        },
        video: {
          type: 'object',
          properties: {
            mp4: {
              type: 'url',
            },
            hls: {
              type: 'url',
              search: { index: 'hls', type: ['TEXT'] },
            },
            pano: {
              type: 'url',
            },
            overlays: {
              type: 'array',
              items: {
                type: 'json', // needs to be json!
                properties: {
                  interval: {
                    type: 'array',
                    items: {
                      type: 'timestamp',
                    },
                  },
                  url: { type: 'url' },
                },
              },
            },
          },
        },
      },
    },
  },
}

const client = connect({ port: 6061 })

start({ port: 6061 }).then((server) => {
  console.log('RUNNING')
  // setTimeout(() => {
  //   const pub = redis.createClient({ port: 6061 })
  //   pub.publish('___selva_lua_logs', 'log log log')
  //   pub.publish('___selva_lua_logs', 'lekker man')
  // }, 500)
  // setTimeout(() => {
  //   server.destroy().catch((e) => {
  //     console.error(e)
  //   })
  // }, 1000)
})

client.updateSchema(schema)
