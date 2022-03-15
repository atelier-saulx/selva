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

test.serial('schemas - hard override', async (t) => {
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

  for (let i = 0; i < 10; i++) {
    const q = []
    for (let i = 0; i < 1000; i++) {
      q.push(
        client.set({
          type: 'thing',
          image: 'flap ' + i,
        })
      )
    }
    await Promise.all(q)
    try {
      if (global.gc) {
        global.gc()
      }
    } catch (err) {
      console.error(`Cannot manualy gc`, err)
    }
  }

  await wait(1000)

  await client.updateSchema(
    {
      types: {
        thing: {
          prefix: 'th',
          fields: {
            flap: {
              type: 'string',
            },
            image: {
              type: 'number',
            },
          },
        },
      },
    },
    'default',
    true,
    (old) => {
      // delete is also a thing
      const num = parseInt(old.image.replace(/[^0-9]/g, ''), 10)
      return {
        image: num || 0,
        flap: old.image,
      }
    }
  )

  const results = await client.get({
    nodes: {
      id: true,
      flap: true,
      image: true,
      $list: {
        $offset: 0,
        $limit: 10,
        $find: {
          $traverse: 'descendants',
          $filter: { $operator: '=', $field: 'type', $value: 'thing' },
        },
      },
    },
  })

  for (const n of results.nodes) {
    t.is(typeof n.image, 'number')
    t.is(typeof n.flap, 'string')
  }

  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()

  t.pass()
})

test.serial('schemas - remove fields', async (t) => {
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

  for (let i = 0; i < 10; i++) {
    const q = []
    for (let i = 0; i < 1000; i++) {
      q.push(
        client.set({
          type: 'thing',
          image: 'flap ' + i,
        })
      )
    }
    await Promise.all(q)
    try {
      if (global.gc) {
        global.gc()
      }
    } catch (err) {
      console.error(`Cannot manualy gc`, err)
    }
  }

  await wait(1000)

  await client.updateSchema(
    {
      types: {
        thing: {
          prefix: 'th',
          fields: {
            flap: {
              type: 'string',
            },
            image: {
              $delete: true,
            },
          },
        },
      },
    },
    'default',
    true,
    (old) => {
      return {
        flap: old.image,
      }
    }
  )

  const results = await client.get({
    nodes: {
      $all: true,
      image: true,
      $list: {
        $offset: 0,
        $limit: 10,
        $find: {
          $traverse: 'descendants',
          $filter: { $operator: '=', $field: 'type', $value: 'thing' },
        },
      },
    },
  })

  for (const n of results.nodes) {
    t.false('image' in n)
    t.is(typeof n.flap, 'string')
  }

  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()

  t.pass()
})

test.serial(
  'schemas - remove/change fields (no mutation handler)',
  async (t) => {
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

    for (let i = 0; i < 10; i++) {
      const q = []
      for (let i = 0; i < 10; i++) {
        q.push(
          client.set(
            i % 2
              ? {
                  type: 'thing',
                  image: 'flap ' + i,
                }
              : { type: 'thing' }
          )
        )
      }
      await Promise.all(q)
    }

    await wait(1000)

    await client.updateSchema(
      {
        types: {
          thing: {
            prefix: 'th',
            fields: {
              flap: {
                type: 'string',
              },
              image: {
                $delete: true,
              },
            },
          },
        },
      },
      'default',
      true
    )

    let results = await client.get({
      nodes: {
        $all: true,
        image: true,
        $list: {
          $offset: 0,
          $limit: 10,
          $find: {
            $traverse: 'descendants',
            $filter: { $operator: '=', $field: 'type', $value: 'thing' },
          },
        },
      },
    })

    for (const n of results.nodes) {
      t.false('image' in n)
    }

    await client.updateSchema(
      {
        types: {
          thing: {
            prefix: 'th',
            fields: {
              flap: {
                type: 'number',
              },
            },
          },
        },
      },
      'default',
      true
    )

    results = await client.get({
      nodes: {
        $all: true,
        image: true,
        $list: {
          $offset: 0,
          $limit: 10,
          $find: {
            $traverse: 'descendants',
            $filter: { $operator: '=', $field: 'type', $value: 'thing' },
          },
        },
      },
    })

    for (const n of results.nodes) {
      t.false('flap' in n)
    }

    await client.destroy()
    await server.destroy()
    await t.connectionsAreEmpty()

    t.pass()
  }
)

test.serial('schemas - return null from mut handler', async (t) => {
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
            flap: {
              type: 'number',
            },
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

  let other = 0

  for (let i = 0; i < 2; i++) {
    const q = []
    for (let i = 0; i < 10; i++) {
      if (i % 2) {
        other++
      }

      q.push(
        client.set(
          i % 2
            ? {
                type: 'thing',
                image: i + '-img',
              }
            : { type: 'thing', flap: i }
        )
      )
    }
    await Promise.all(q)
  }

  await wait(1000)

  await client.updateSchema(
    {
      types: {
        thing: {
          prefix: 'th',
          fields: {
            flap: {
              type: 'string',
            },
            image: {
              $delete: true,
            },
          },
        },
      },
    },
    'default',
    true,
    (node) => {
      if (node.image) {
        return {
          flap: '10000',
        }
      }
    }
  )

  let results = await client.get({
    nodes: {
      $all: true,
      $list: {
        $offset: 0,
        $limit: 100,
        $find: {
          $traverse: 'descendants',
          $filter: { $operator: '=', $field: 'type', $value: 'thing' },
        },
      },
    },
  })

  let amount10k = 0
  for (const n of results.nodes) {
    t.false('image' in n)
    if (n.flap === '10000') {
      amount10k++
    }
  }

  t.is(amount10k, other)

  await client.updateSchema(
    {
      types: {
        thing: {
          prefix: 'th',
          fields: {
            flap: {
              type: 'number',
            },
          },
        },
      },
    },
    'default',
    true,
    (node) => {
      if (node.flap === '10000') {
        return {
          flap: 10000,
        }
      }
    }
  )

  results = await client.get({
    nodes: {
      $all: true,
      image: true,
      $list: {
        $offset: 0,
        $limit: 100,
        $find: {
          $traverse: 'descendants',
          $filter: { $operator: '=', $field: 'type', $value: 'thing' },
        },
      },
    },
  })

  amount10k = 0

  for (const n of results.nodes) {
    if (n.flap === 10000) {
      amount10k++
    } else {
      t.false('flap' in n)
    }
  }

  t.is(amount10k, other)

  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()

  t.pass()
})

test.serial('schemas - return to other id or type', async (t) => {
  const port = await getPort()
  const server = await start({
    port,
  })
  const client = connect({ port })

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

  await wait(1000)

  const q = []
  for (let i = 0; i < 10; i++) {
    q.push(
      client.set({
        type: 'thing',
        image: i + '-img',
      })
    )
  }

  await Promise.all(q)

  await wait(1000)

  await client.updateSchema(
    {
      types: {
        flap: {
          fields: {
            image: {
              type: 'string',
            },
          },
        },
        thing: {
          prefix: 'th',
          fields: {
            image: {
              $delete: true,
            },
          },
        },
      },
    },
    'default',
    true,
    (node) => {
      return {
        type: 'flap',
        image: node.image,
      }
    }
  )

  const results = await client.get({
    nodes: {
      $all: true,
      $list: {
        $offset: 0,
        $limit: 100,
        $find: {
          $traverse: 'descendants',
          $filter: { $operator: '=', $field: 'type', $value: 'thing' },
        },
      },
    },
  })

  for (const n of results.nodes) {
    t.false('image' in n)
  }

  const results2 = await client.get({
    nodes: {
      $all: true,
      $list: {
        $offset: 0,
        $limit: 100,
        $find: {
          $traverse: 'descendants',
          $filter: { $operator: '=', $field: 'type', $value: 'flap' },
        },
      },
    },
  })

  for (const n of results2.nodes) {
    t.true('image' in n)
  }

  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()

  t.pass()
})

test.serial('schemas - migrate type', async (t) => {
  const port = await getPort()
  const server = await start({
    port,
  })
  const client = connect({ port })

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

  await wait(1000)

  const q = []
  for (let i = 0; i < 10; i++) {
    q.push(
      client.set({
        type: 'thing',
        image: i + '-img',
      })
    )
  }

  await Promise.all(q)

  await wait(1000)

  await client.updateSchema(
    {
      types: {
        flap: {
          fields: {
            image: {
              type: 'string',
            },
          },
        },
        thing: {
          $delete: true,
        },
      },
    },
    'default',
    true,
    (node) => {
      // needs to get ALL fields including refs
      return {
        parents: node.parents,
        type: 'flap',
        image: node.image,
      }
    }
  )

  try {
    await client.get({
      nodes: {
        $all: true,
        $list: {
          $offset: 0,
          $limit: 100,
          $find: {
            $traverse: 'descendants',
            $filter: { $operator: '=', $field: 'type', $value: 'thing' },
          },
        },
      },
    })
  } catch (err) {
    // has to crash and say no
    console.info(err)
  }

  const results2 = await client.get({
    nodes: {
      $all: true,
      $list: {
        $offset: 0,
        $limit: 100,
        $find: {
          $traverse: 'descendants',
          $filter: { $operator: '=', $field: 'type', $value: 'flap' },
        },
      },
    },
  })

  for (const n of results2.nodes) {
    t.true('image' in n)
  }

  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()

  t.pass()
})

test.serial('schemas - migrate object', async (t) => {
  const port = await getPort()
  const server = await start({
    port,
  })
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en'],
    types: {
      thing: {
        prefix: 'th',
        fields: {
          x: {
            type: 'object',
            properties: {
              y: {
                type: 'object',
                properties: {
                  z: {
                    type: 'object',
                    properties: {
                      rank: {
                        type: 'number',
                      },
                      image: {
                        type: 'string',
                      },
                    },
                  },
                },
              },
            },
          },
        },
      },
    },
  })

  await wait(1000)

  const q = []
  for (let i = 0; i < 10; i++) {
    q.push(
      client.set({
        type: 'thing',
        x:
          i % 2
            ? {
                y: {
                  z: { image: i + '-img', rank: i * 10 },
                },
              }
            : { y: { z: { rank: i } } },
      })
    )
  }

  await Promise.all(q)

  await wait(1000)

  await client.updateSchema(
    {
      types: {
        thing: {
          fields: {
            x: {
              type: 'object',
              properties: {
                y: {
                  type: 'object',
                  properties: {
                    z: {
                      type: 'object',
                      properties: {
                        image: {
                          $delete: true,
                        },
                        rank: {
                          type: 'string',
                        },
                        flap: {
                          type: 'string',
                        },
                      },
                    },
                  },
                },
              },
            },
          },
        },
      },
    },
    'default',
    true,
    (node) => {
      if (node.x?.y?.z?.image) {
        return {
          x: { y: { z: { flap: node.x.y.z.image, rank: 'THE BEST' } } },
        }
      }
    }
  )

  const results = await client.get({
    nodes: {
      $all: true,
      $list: {
        $offset: 0,
        $limit: 100,
        $find: {
          $traverse: 'descendants',
          $filter: { $operator: '=', $field: 'type', $value: 'thing' },
        },
      },
    },
  })

  let imgCounter = 0
  let rankCounter = 0

  for (const n of results.nodes) {
    if (n.x?.y?.z) {
      if (n.x?.y?.z.flap) {
        imgCounter++
      }
      if (n.x?.y?.z.rank) {
        rankCounter++
      }
    }
  }

  t.is(rankCounter, 5)
  t.is(imgCounter, 5)

  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()

  t.pass()
})

test.only('schemas - validate array', async (t) => {
  const port = await getPort()
  const server = await start({
    port,
  })
  const client = connect({ port })

  t.throwsAsync(
    client.updateSchema({
      languages: ['en'],
      types: {
        thing: {
          prefix: 'th',
          fields: {
            list: {
              // @ts-ignore
              type: 'array',
              fields: {
                type: 'object',
                properties: {
                  flap: { type: 'number' },
                },
              },
            },
          },
        },
      },
    })
  )

  console.info('hello')

  await wait(1000)

  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()

  t.pass()
})
