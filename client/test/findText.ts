import test from 'ava'
import fetch from 'node-fetch'
import { connect } from '../src/index'
import { start, startTextServer } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let txtSrv
let port
const txtPort = 33333
test.before(async (t) => {
  port = await getPort()
  // txtPort = await getPort()
  srv = await start({
    port,
  })

  txtSrv = startTextServer({ port: txtPort })

  await wait(500)
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'de', 'nl', 'it'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          title: { type: 'text' },
          value: { type: 'number' },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          value: { type: 'number' },
        },
      },
      ticket: {
        prefix: 'tk',
        fields: {
          title: { type: 'text' },
          name: { type: 'string' },
        },
      },
    },
  })

  await wait(100)
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  //txtSrv.stop()
  await t.connectionsAreEmpty()
})

test.serial('find fields with a substring match', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'root',
    children: [
      {
        type: 'ticket',
        title: { en: 'Game One' },
        name: 'Amanpreet Bennett',
      },
      {
        type: 'ticket',
        title: { en: 'Game One' },
        name: 'Ozan Weston'
      },
      {
        type: 'ticket',
        title: { en: 'Game One' },
        name: 'Alejandro Hackett'
      },
      {
        type: 'ticket',
        title: { en: 'Game Two' },
        name: 'Dane Bray',
      },
      {
        type: 'ticket',
        title: { en: 'Game One' },
        name: 'Lyndsey Hackett',
      },
      {
        type: 'ticket',
        title: { en: 'Game One' },
        name: 'Chandler Hackett',
      },
      {
        type: 'ticket',
        title: { en: 'Game Two' },
        name: 'Harold Pate',
      },
      {
        type: 'ticket',
        title: { en: 'Game One' },
        name: 'Stella Cisneros',
      },
      {
        type: 'ticket',
        title: { en: 'Game Two' },
        name: 'Norman Hackett',
      },
      {
        type: 'ticket',
        title: { en: 'Game One' },
        name: 'Rikesh Frey'
      }
    ]
  })

  t.deepEqual(await client.get({
    descendants: {
      title: true,
      name: true,
      $list: {
        $sort: { $field: 'name', $order: 'asc' },
        $find: {
          $filter: [
            {
              $operator: 'includes',
              $field: 'name',
              $value: 'Hackett',
            },
          ],
        },
      },
    },
  }),
  {
    descendants: [
      {
        title: { "en": "Game One" },
        name: "Alejandro Hackett"
      },
      {
        title: { "en": "Game One" },
        name: "Chandler Hackett"
      },
      {
        title: { "en": "Game One" },
        name: "Lyndsey Hackett"
      },
      {
        title: { "en": "Game Two" },
        name: "Norman Hackett"
      },
    ]
  })

  t.deepEqual(await client.get({
    $language: 'en',
    descendants: {
      title: true,
      name: true,
      $list: {
        $sort: { $field: 'name', $order: 'asc' },
        $find: {
          $filter: [
            {
              $operator: 'includes',
              $field: 'title',
              $value: 'One',
            },
          ],
        },
      },
    },
  }),
  {
    descendants: [
      {
        name: 'Alejandro Hackett',
        title: 'Game One',
      },
      {
        name: 'Amanpreet Bennett',
        title: 'Game One',
      },
      {
        name: 'Chandler Hackett',
        title: 'Game One',
      },
      {
        name: 'Lyndsey Hackett',
        title: 'Game One',
      },
      {
        name: 'Ozan Weston',
        title: 'Game One',
      },
      {
        name: 'Rikesh Frey',
        title: 'Game One',
      },
      {
        name: 'Stella Cisneros',
        title: 'Game One',
      },
    ]
  })

  await client.destroy()
})

test.serial.failing('hhnn', async (t) => {
  let resp = await fetch(`http://localhost:${txtPort}/set`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json',
    },
    body: JSON.stringify({
      $id: 'maga',
      $searchString: 'hello world',
      $field: 'title',
      $language: 'en',
    }),
  })

  console.log('YES', await resp.text())

  resp = await fetch(`http://localhost:${txtPort}/set`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json',
    },
    body: JSON.stringify({
      $id: 'masa',
      $searchString: 'hell song',
      $field: 'title',
      $language: 'en',
    }),
  })

  console.log('YES', await resp.text())

  const client = connect({ port })

  await client.set({
    $language: 'en',
    $id: 'maga',
    title: 'hello world',
    value: 10,
  })

  await client.set({
    $language: 'en',
    $id: 'masa',
    title: 'hell song',
    value: 11,
  })

  const res = await client.get({
    descendants: {
      id: true,
      title: true,
      value: true,
      $list: {
        $find: {
          $filter: [
            {
              $operator: 'textSearch',
              $field: 'title',
              $value: 'hel',
            },
          ],
        },
      },
    },
  })

  console.log('YES', res)

  await client.destroy()
})

// test.serial.only('hmm', async t => {
//   let resp = await fetch(`http://localhost:${txtPort}/set`, {
//     method: 'POST',
//     headers: {
//       'content-type': 'application/json'
//     },
//     body: JSON.stringify({
//       $id: 'maga',
//       $searchString: 'hello world',
//       $field: 'title',
//       $language: 'en'
//     })
//   })
//
//   console.log('YES', await resp.text())
//
//   resp = await fetch(`http://localhost:${txtPort}/set`, {
//     method: 'POST',
//     headers: {
//       'content-type': 'application/json'
//     },
//     body: JSON.stringify({
//       $id: 'masa',
//       $searchString: 'hell song',
//       $field: 'title',
//       $language: 'en'
//     })
//   })
//
//   console.log('YES', await resp.text())
//
//   resp = await fetch(`http://localhost:${txtPort}/get`, {
//     method: 'POST',
//     headers: {
//       'content-type': 'application/json'
//     },
//     body: JSON.stringify({
//       $searchString: 'hell',
//       $field: 'title',
//       $language: 'en'
//     })
//   })
//
//   console.log('YES', await resp.text())
//
//   resp = await fetch(`http://localhost:${txtPort}/get`, {
//     method: 'POST',
//     headers: {
//       'content-type': 'application/json'
//     },
//     body: JSON.stringify({
//       $searchString: 'wor',
//       $field: 'title',
//       $language: 'en'
//     })
//   })
//
//   console.log('YES', await resp.text())
// })

// TODO: this needs to use a non-TEXT-lANGUAGE-SUG field
test.serial.skip('find - exact text match on exact field', async (t) => {
  // simple nested - single query
  const client = connect({ port }, { loglevel: 'info' })
  await client.set({
    type: 'match',
    name: 'match 1',
    title: {
      en: 'a nice match',
    },
  })

  await client.set({
    type: 'match',
    name: 'match 2',
    title: {
      en: 'greatest match',
    },
  })

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'en',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'match',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: 'greatest match',
                },
              ],
            },
          },
        },
      })
    ).items.map((x) => x.name),
    ['match 2']
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'en',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'match',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: 'nice match',
                },
              ],
            },
          },
        },
      })
    ).items.map((x) => x.name),
    ['match 1']
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'en',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'match',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: 'match',
                },
              ],
            },
          },
        },
      })
    ).items.map((x) => x.name),
    ['match 1', 'match 2']
  )

  await client.delete('root')
  await client.destroy()
})

test.serial.skip('find - find with suggestion', async (t) => {
  // simple nested - single query
  const client = connect({ port }, { loglevel: 'info' })
  await client.set({
    type: 'league',
    name: 'league 1',
    title: {
      en: 'a nice league',
    },
  })

  await client.set({
    type: 'league',
    name: 'league 2',
    title: {
      en: 'greatest league',
    },
  })

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'en',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'league',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: 'great',
                },
              ],
            },
          },
        },
      })
    ).items.map((x) => x.name),
    ['league 2']
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'en',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'league',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: 'nic',
                },
              ],
            },
          },
        },
      })
    ).items.map((x) => x.name),
    ['league 1']
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'en',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'league',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: 'league',
                },
              ],
            },
          },
        },
      })
    ).items.map((x) => x.name),
    ['league 1', 'league 2']
  )

  await client.delete('root')
  await client.destroy()
})

test.serial.skip(
  'find - find with suggestion containing special characters',
  async (t) => {
    // simple nested - single query
    const client = connect({ port }, { loglevel: 'info' })
    await client.set({
      type: 'league',
      name: 'league 1',
      title: {
        en: 'äitin mussukoiden nappula liiga 😂👌',
      },
    })

    await client.set({
      type: 'league',
      name: 'league 2',
      title: {
        en: '🐂 münchen mädness liiga 💥',
      },
    })

    t.deepEqualIgnoreOrder(
      (
        await client.get({
          $id: 'root',
          $language: 'en',
          id: true,
          items: {
            name: true,
            $list: {
              $find: {
                $traverse: 'children',
                $filter: [
                  {
                    $field: 'type',
                    $operator: '=',
                    $value: 'league',
                  },
                  {
                    $field: 'title',
                    $operator: '=',
                    $value: 'munch',
                  },
                ],
              },
            },
          },
        })
      ).items.map((x) => x.name),
      ['league 2']
    )

    t.deepEqualIgnoreOrder(
      (
        await client.get({
          $id: 'root',
          $language: 'en',
          id: true,
          items: {
            name: true,
            $list: {
              $find: {
                $traverse: 'children',
                $filter: [
                  {
                    $field: 'type',
                    $operator: '=',
                    $value: 'league',
                  },
                  {
                    $field: 'title',
                    $operator: '=',
                    $value: 'madn',
                  },
                ],
              },
            },
          },
        })
      ).items.map((x) => x.name),
      ['league 2']
    )

    t.deepEqualIgnoreOrder(
      (
        await client.get({
          $id: 'root',
          $language: 'en',
          id: true,
          items: {
            name: true,
            $list: {
              $find: {
                $traverse: 'children',
                $filter: [
                  {
                    $field: 'type',
                    $operator: '=',
                    $value: 'league',
                  },
                  {
                    $field: 'title',
                    $operator: '=',
                    $value: 'aiti',
                  },
                ],
              },
            },
          },
        })
      ).items.map((x) => x.name),
      ['league 1']
    )

    t.deepEqualIgnoreOrder(
      (
        await client.get({
          $id: 'root',
          $language: 'en',
          id: true,
          items: {
            name: true,
            $list: {
              $find: {
                $traverse: 'children',
                $filter: [
                  {
                    $field: 'type',
                    $operator: '=',
                    $value: 'league',
                  },
                  {
                    $field: 'title',
                    $operator: '=',
                    $value: 'liiga',
                  },
                ],
              },
            },
          },
        })
      ).items.map((x) => x.name),
      ['league 1', 'league 2']
    )

    await client.delete('root')
    await client.destroy()
  }
)

test.serial.skip('find - find with another language', async (t) => {
  // simple nested - single query
  const client = connect({ port }, { loglevel: 'info' })
  const l1 = await client.set({
    type: 'league',
    name: 'league 1',
    title: {
      // en: 'yes nice league',
      nl: 'yesh mooie competitie',
      it: 'pallacanestro',
    },
  })

  const l2 = await client.set({
    type: 'league',
    name: 'league 2',
    title: {
      de: 'yesh german league',
    },
  })

  await client.set({
    $id: l1,
    type: 'league',
    name: 'league 1',
    title: {
      en: 'yes nice league',
    },
  })

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'en',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'league',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: 'nice',
                },
              ],
            },
          },
        },
      })
    ).items.map((x) => x.name),
    ['league 1']
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'nl',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'league',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: 'mooie',
                },
              ],
            },
          },
        },
      })
    ).items.map((x) => x.name),
    ['league 1']
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'de',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'league',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: 'nice',
                },
              ],
            },
          },
        },
      })
    ).items.map((x) => x.name),
    ['league 1']
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'en',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'league',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: 'german',
                },
              ],
            },
          },
        },
      })
    ).items.map((x) => x.name),
    ['league 2']
  )

  await client.set({
    $id: l2,
    title: {
      en: 'yesh en league',
    },
  })

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'en',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'league',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: 'german',
                },
              ],
            },
          },
        },
      })
    ).items.map((x) => x.name),
    []
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'de',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'league',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: 'german',
                },
              ],
            },
          },
        },
      })
    ).items.map((x) => x.name),
    ['league 2']
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'nl',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'league',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: 'en league',
                },
              ],
            },
          },
        },
      })
    ).items.map((x) => x.name),
    ['league 2']
  )

  await client.delete('root')
  await client.destroy()
})

test.serial.skip(
  'find - find with suggestion starting with whitespace',
  async (t) => {
    // simple nested - single query
    const client = connect({ port }, { loglevel: 'info' })
    await client.set({
      type: 'league',
      name: 'league 1',
      title: {
        en: ' a nice league',
      },
    })

    await client.set({
      type: 'league',
      name: 'league 2',
      title: {
        en: '  greatest   league',
      },
    })

    t.deepEqualIgnoreOrder(
      (
        await client.get({
          $id: 'root',
          $language: 'en',
          id: true,
          items: {
            name: true,
            $list: {
              $find: {
                $traverse: 'children',
                $filter: [
                  {
                    $field: 'type',
                    $operator: '=',
                    $value: 'league',
                  },
                  {
                    $field: 'title',
                    $operator: '=',
                    $value: ' great',
                  },
                ],
              },
            },
          },
        })
      ).items.map((x) => x.name),
      ['league 2']
    )

    t.deepEqualIgnoreOrder(
      (
        await client.get({
          $id: 'root',
          $language: 'en',
          id: true,
          items: {
            name: true,
            $list: {
              $find: {
                $traverse: 'children',
                $filter: [
                  {
                    $field: 'type',
                    $operator: '=',
                    $value: 'league',
                  },
                  {
                    $field: 'title',
                    $operator: '=',
                    $value: '   nic     ',
                  },
                ],
              },
            },
          },
        })
      ).items.map((x) => x.name),
      ['league 1']
    )

    t.deepEqualIgnoreOrder(
      (
        await client.get({
          $id: 'root',
          $language: 'en',
          id: true,
          items: {
            name: true,
            $list: {
              $find: {
                $traverse: 'children',
                $filter: [
                  {
                    $field: 'type',
                    $operator: '=',
                    $value: 'league',
                  },
                  {
                    $field: 'title',
                    $operator: '=',
                    $value: '   league',
                  },
                ],
              },
            },
          },
        })
      ).items.map((x) => x.name),
      ['league 1', 'league 2']
    )

    try {
      await client.get({
        $id: 'root',
        $language: 'en',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'league',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: '*  ',
                },
              ],
            },
          },
        },
      })

      await client.get({
        $id: 'root',
        $language: 'en',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'league',
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: '',
                },
              ],
            },
          },
        },
      })
    } catch (e) {
      console.error(e)
      t.fail()
    }

    await client.delete('root')
    await client.destroy()
  }
)
