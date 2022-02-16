import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

test.serial('schemas - custom validation', async (t) => {
  const port = await getPort()
  const server = await start({
    port,
  })
  const client = connect({ port })

  try {
    await client.updateSchema({
      languages: ['en'],
      types: {
        thing: {
          prefix: 'th',
          fields: {
            image: {
              type: 'string',
              meta: 'image',
            },
            set: {
              type: 'set',
              items: {
                type: 'string',
              },
            },
            list: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  x: { type: 'number' },
                  y: { type: 'number' },
                },
              },
            },
            t: {
              type: 'text',
            },
            snurky: {
              type: 'references',
            },
            flara: {
              type: 'reference',
              bidirectional: {
                fromField: 'flarb',
              },
            },
            flarb: {
              type: 'reference',
              bidirectional: {
                fromField: 'flara',
              },
            },
            obj: {
              type: 'object',
              properties: {
                flap: {
                  type: 'string',
                },
                x: {
                  type: 'object',
                  properties: {
                    snurk: {
                      type: 'string',
                    },
                  },
                },
              },
            },
          },
        },
      },
    })
  } catch (err) {}

  const info = []

  client.setCustomValidator((schema, type, path, value, lang) => {
    info.push([type, ...path])
    return true
  })

  const id = await client.set({ type: 'thing' })

  // high level validator
  await client.set({
    type: 'thing',
    image: 'yes',
    list: [
      {
        x: 1,
        y: 1,
      },
    ],
    flara: id,
    obj: {
      flap: 'x',
      x: {
        snurk: 'hello',
      },
    },
    t: {
      en: 'flap',
    },
    set: ['yes'],
    children: {
      $add: [id],
    },
  })

  t.deepEqual(info, [
    ['thing', 'type'],
    ['thing', 'type'],
    ['thing', 'image'],
    ['thing', 'list'],
    ['thing', 'list', 0],
    ['thing', 'list', 0, 'x'],
    ['thing', 'list', 0, 'y'],
    ['thing', 'flara'],
    ['thing', 'obj'],
    ['thing', 'obj', 'flap'],
    ['thing', 'obj', 'x'],
    ['thing', 'obj', 'x', 'snurk'],
    ['thing', 't'],
    ['thing', 'set'],
    ['thing', 'value'],
    ['thing', 'children'],
  ])

  await wait(1000)

  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()
})

// serial
test.only('schemas - hard override', async (t) => {
  const port = await getPort()
  const server = await start({
    port,
  })
  const client = connect({ port })

  try {
    await client.updateSchema({
      languages: ['en'],
      types: {
        thing: {
          prefix: 'th',
          fields: {
            image: {
              type: 'string',
            },
          },
        },
      },
    })
  } catch (err) {
    console.info('--', err)
  }

  await wait(1000)

  t.throwsAsync(
    client.updateSchema({
      types: {
        thing: {
          prefix: 'th',
          fields: {
            image: {
              type: 'number',
            },
          },
        },
      },
    })
  )

  const q = []
  for (let i = 0; i < 10e3; i++) {
    q.push(
      client.set({
        type: 'thing',
        image: 'flap',
      })
    )
  }

  await Promise.all(q)

  await wait(1000)

  await client.updateSchema(
    {
      types: {
        thing: {
          prefix: 'th',
          fields: {
            image: {
              type: 'number',
            },
          },
        },
      },
    },
    'default',
    true
    // (old) => {
    //   console.info('hello old', old)
    //   return {
    //     image: '!' + old.image,
    //   }
    // }
  )

  const x = await client.get({
    nodes: {
      id: true,
      image: true,
      $list: {
        $offset: 0,
        $limit: 5000,
        $find: {
          $traverse: 'descendants',
          $filter: { $operator: '=', $field: 'type', $value: 'thing' },
        },
      },
    },
  })

  console.log(x)

  // batch per 5k
  // (old) => {
  // if non existing remove field
  //   return { type: old.type, image: '!' + old.image }
  // }

  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()

  t.pass()
})
