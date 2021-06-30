import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async (t) => {
  port = await getPort()
  srv = await start({ port })
  await wait(500)
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('get - simple $list', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      custom: {
        prefix: 'cu',
        fields: {
          value: { type: 'number', search: true },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
    },
  })

  const children = []

  for (let i = 0; i < 100; i++) {
    children.push({
      type: 'custom',
      value: i,
      name: 'flurp' + i,
    })
  }

  await Promise.all([
    client.set({
      $id: 'cuA',
      image: {
        thumb: 'flurp.jpg',
      },
      title: { en: 'snurf' },
      children,
    }),
  ])

  const c = await client.get({
    $id: 'cuA',
    children: {
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $limit: 10,
      },
    },
  })

  t.deepEqual(
    c,
    {
      children: [
        { value: 0, name: 'flurp0' },
        { value: 1, name: 'flurp1' },
        { value: 2, name: 'flurp2' },
        { value: 3, name: 'flurp3' },
        { value: 4, name: 'flurp4' },
        { value: 5, name: 'flurp5' },
        { value: 6, name: 'flurp6' },
        { value: 7, name: 'flurp7' },
        { value: 8, name: 'flurp8' },
        { value: 9, name: 'flurp9' },
      ],
    },
    'non redis search sort'
  )

  const { children: rangeResult } = await client.get({
    $id: 'cuA',
    children: {
      name: true,
      value: true,
      $list: {
        $limit: 10,
      },
    },
  })

  t.is(rangeResult.length, 10, 'non redis search range')

  /*
  const x = await client.get({
    $id: 'cuA',
    related: {
      $inherit: true,
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $range: [0, 10]
      }
    }
  })
  */

  await client.destroy()
})

test.serial('get - simple $list with $field of one field', async (t) => {
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      custom: {
        prefix: 'cu',
        fields: {
          name: { type: 'string' },
          value: { type: 'number', search: true },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
    },
  })

  const children = []

  for (let i = 0; i < 100; i++) {
    children.push({
      type: 'custom',
      value: i,
      name: 'flurp' + i,
    })
  }

  await Promise.all([
    client.set({
      $id: 'cuA',
      image: {
        thumb: 'flurp.jpg',
      },
      title: { en: 'snurf' },
      children,
    }),
  ])

  const c = await client.get({
    $id: 'cuA',
    otherName: {
      name: true,
      value: true,
      $field: 'children',
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $limit: 10,
      },
    },
  })

  t.deepEqual(
    c,
    {
      otherName: [
        { value: 0, name: 'flurp0' },
        { value: 1, name: 'flurp1' },
        { value: 2, name: 'flurp2' },
        { value: 3, name: 'flurp3' },
        { value: 4, name: 'flurp4' },
        { value: 5, name: 'flurp5' },
        { value: 6, name: 'flurp6' },
        { value: 7, name: 'flurp7' },
        { value: 8, name: 'flurp8' },
        { value: 9, name: 'flurp9' },
      ],
    },
    'non redis search sort'
  )

  await client.destroy()
})

test.serial(
  'get - simple $list with $field of two field entries',
  async (t) => {
    const client = connect({ port })

    await client.updateSchema({
      languages: ['en', 'de', 'nl'],
      types: {
        custom: {
          prefix: 'cu',
          fields: {
            name: { type: 'string' },
            value: { type: 'number', search: true },
            age: { type: 'number' },
            auth: {
              type: 'json',
            },
            related: { type: 'references' },
            title: { type: 'text' },
            description: { type: 'text' },
            image: {
              type: 'object',
              properties: {
                thumb: { type: 'string' },
                poster: { type: 'string' },
              },
            },
          },
        },
      },
    })

    const children = []

    for (let i = 0; i < 100; i++) {
      children.push({
        type: 'custom',
        value: i,
        name: 'flurp' + i,
      })
    }

    await Promise.all([
      client.set({
        $id: 'cuA',
        image: {
          thumb: 'flurp.jpg',
        },
        title: { en: 'snurf' },
        children,
      }),
    ])

    const c = await client.get({
      $id: 'cuA',
      otherName: {
        name: true,
        value: true,
        $field: ['related', 'children'],
        $list: {
          $sort: { $field: 'value', $order: 'asc' },
          $limit: 10,
        },
      },
    })

    t.deepEqual(
      c,
      {
        otherName: [
          { value: 0, name: 'flurp0' },
          { value: 1, name: 'flurp1' },
          { value: 2, name: 'flurp2' },
          { value: 3, name: 'flurp3' },
          { value: 4, name: 'flurp4' },
          { value: 5, name: 'flurp5' },
          { value: 6, name: 'flurp6' },
          { value: 7, name: 'flurp7' },
          { value: 8, name: 'flurp8' },
          { value: 9, name: 'flurp9' },
        ],
      },
      'non redis search sort'
    )
  }
)

// FIXME: yes?
test.serial.skip(
  'get - simple $list with query $field of one field',
  async (t) => {
    const client = connect({ port }, { loglevel: 'info' })

    await client.updateSchema({
      languages: ['en', 'de', 'nl'],
      types: {
        custom: {
          prefix: 'cu',
          fields: {
            name: { type: 'string' },
            value: { type: 'number', search: true },
            age: { type: 'number' },
            auth: {
              type: 'json',
            },
            title: { type: 'text' },
            description: { type: 'text' },
            image: {
              type: 'object',
              properties: {
                thumb: { type: 'string' },
                poster: { type: 'string' },
              },
            },
          },
        },
      },
    })

    const children = []

    for (let i = 0; i < 100; i++) {
      children.push({
        type: 'custom',
        value: i,
        name: 'flurp' + i,
      })
    }

    await Promise.all([
      client.set({
        $id: 'cuA',
        image: {
          thumb: 'flurp.jpg',
        },
        title: { en: 'snurf' },
        children,
      }),
    ])

    const c = await client.get({
      $id: 'cuB',
      otherName: {
        name: true,
        value: true,
        $field: { path: 'children', value: { $id: 'cuA', children: true } },
        $list: {
          $sort: { $field: 'value', $order: 'asc' },
          $limit: 10,
        },
      },
    })

    t.deepEqual(
      c,
      {
        $isNull: true,
        otherName: [
          { value: 0, name: 'flurp0' },
          { value: 1, name: 'flurp1' },
          { value: 2, name: 'flurp2' },
          { value: 3, name: 'flurp3' },
          { value: 4, name: 'flurp4' },
          { value: 5, name: 'flurp5' },
          { value: 6, name: 'flurp6' },
          { value: 7, name: 'flurp7' },
          { value: 8, name: 'flurp8' },
          { value: 9, name: 'flurp9' },
        ],
      },
      'non redis search sort'
    )

    const c2 = await client.get({
      $id: 'cuB',
      otherName: {
        name: true,
        value: true,
        $field: { path: 'children', value: { $id: 'cuA', children: true } },
        $list: true,
      },
    })

    t.is(c2.otherName.length, 100, 'list true')
    await client.destroy()
  }
)

test.serial('get - simple $list nested query structure', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      custom: {
        prefix: 'cu',
        fields: {
          value: { type: 'number', search: true },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
    },
  })

  const children = []

  for (let i = 0; i < 100; i++) {
    children.push({
      type: 'custom',
      value: i,
      name: 'flurp' + i,
    })
  }

  await Promise.all([
    client.set({
      $id: 'cuA',
      image: {
        thumb: 'flurp.jpg',
      },
      title: { en: 'snurf' },
      children,
    }),
  ])

  let c = await client.get({
    $id: 'cuA',
    hello: {
      yesyes: {
        children: {
          $field: 'children',
          name: true,
          value: true,
          $list: {
            $sort: { $field: 'value', $order: 'asc' },
            $limit: 10,
          },
        },
      },
    },
  })

  t.deepEqual(
    c,
    {
      hello: {
        yesyes: {
          children: [
            { value: 0, name: 'flurp0' },
            { value: 1, name: 'flurp1' },
            { value: 2, name: 'flurp2' },
            { value: 3, name: 'flurp3' },
            { value: 4, name: 'flurp4' },
            { value: 5, name: 'flurp5' },
            { value: 6, name: 'flurp6' },
            { value: 7, name: 'flurp7' },
            { value: 8, name: 'flurp8' },
            { value: 9, name: 'flurp9' },
          ],
        },
      },
    },
    'non redis search sort'
  )

  c = await client.get({
    $id: 'cuA',
    hello: {
      yesyes: {
        children: {
          $field: 'children',
          name: true,
          value: true,
          $list: {
            $sort: { $field: 'value', $order: 'asc' },
            $limit: 10,
            $offset: 10,
          },
        },
      },
    },
  })

  t.deepEqual(
    c,
    {
      hello: {
        yesyes: {
          children: [
            { value: 10, name: 'flurp10' },
            { value: 11, name: 'flurp11' },
            { value: 12, name: 'flurp12' },
            { value: 13, name: 'flurp13' },
            { value: 14, name: 'flurp14' },
            { value: 15, name: 'flurp15' },
            { value: 16, name: 'flurp16' },
            { value: 17, name: 'flurp17' },
            { value: 18, name: 'flurp18' },
            { value: 19, name: 'flurp19' },
          ],
        },
      },
    },
    'non redis search sort'
  )

  const { children: rangeResult } = await client.get({
    $id: 'cuA',
    children: {
      name: true,
      value: true,
      $list: {
        $limit: 10,
      },
    },
  })

  t.is(rangeResult.length, 10, 'non redis search range')

  /*
  const x = await client.get({
    $id: 'cuA',
    related: {
      $inherit: true,
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $range: [0, 10]
      }
    }
  })
  */

  await client.destroy()
})

test.serial('get - default sorting in $list with references', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      custom: {
        prefix: 'cu',
        fields: {
          value: { type: 'number', search: true },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
    },
  })

  const children = []

  for (let i = 0; i < 100; i++) {
    children.push({
      $id: 'cu' + String(i).padStart(3, '0'),
      type: 'custom',
      value: i,
      name: 'flurp' + i,
    })
  }

  await Promise.all([
    client.set({
      $id: 'cuA',
      image: {
        thumb: 'flurp.jpg',
      },
      title: { en: 'snurf' },
      children,
    }),
  ])

  let c = await client.get({
    $id: 'cuA',
    children: {
      name: true,
      value: true,
      $list: {
        $limit: 10,
      },
    },
  })

  t.deepEqual(
    c,
    {
      children: [
        { value: 0, name: 'flurp0' },
        { value: 1, name: 'flurp1' },
        { value: 2, name: 'flurp2' },
        { value: 3, name: 'flurp3' },
        { value: 4, name: 'flurp4' },
        { value: 5, name: 'flurp5' },
        { value: 6, name: 'flurp6' },
        { value: 7, name: 'flurp7' },
        { value: 8, name: 'flurp8' },
        { value: 9, name: 'flurp9' },
      ],
    },
    'non redis search sort'
  )

  c = await client.get({
    $id: 'cuA',
    otherName: {
      name: true,
      value: true,
      $field: ['related', 'children'],
      $list: {
        $offset: 10,
        $sort: { $field: 'value', $order: 'asc' },
        $limit: 10,
      },
    },
  })

  t.deepEqual(
    c,
    {
      otherName: [
        { value: 10, name: 'flurp10' },
        { value: 11, name: 'flurp11' },
        { value: 12, name: 'flurp12' },
        { value: 13, name: 'flurp13' },
        { value: 14, name: 'flurp14' },
        { value: 15, name: 'flurp15' },
        { value: 16, name: 'flurp16' },
        { value: 17, name: 'flurp17' },
        { value: 18, name: 'flurp18' },
        { value: 19, name: 'flurp19' },
      ],
    },
    'non redis search sort'
  )

  /*
  const x = await client.get({
    $id: 'cuA',
    related: {
      $inherit: true,
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $range: [0, 10]
      }
    }
  })
  */
  await client.destroy()
})

// TODO: BETTER SORTING
// test.serial.only('get - sort is case sensivite', async (t) => {
//   const client = connect({ port }, { loglevel: 'info' })
//
//   await client.updateSchema({
//     languages: ['en', 'de', 'nl'],
//     types: {
//       custom: {
//         prefix: 'cu',
//         fields: {
//           value: { type: 'number', search: true },
//           age: { type: 'number' },
//           auth: {
//             type: 'json',
//           },
//           title: { type: 'text' },
//           name: { type: 'string' },
//           description: { type: 'text' },
//           image: {
//             type: 'object',
//             properties: {
//               thumb: { type: 'string' },
//               poster: { type: 'string' },
//             },
//           },
//         },
//       },
//     },
//   })
//
//   const children = []
//
//   for (let i = 0; i < 100; i++) {
//     children.push({
//       type: 'custom',
//       value: i,
//       name: 'flurp' + i,
//     })
//   }
//
//   children.push(
//     await client.set({
//       type: 'custom',
//       value: 999,
//       name: 'Flurp' + 999,
//     })
//   )
//
//   await Promise.all([
//     client.set({
//       $id: 'cuA',
//       image: {
//         thumb: 'flurp.jpg',
//       },
//       title: { en: 'snurf' },
//       children,
//     }),
//   ])
//
//   const c = await client.get({
//     $id: 'cuA',
//     children: {
//       name: true,
//       value: true,
//       $list: {
//         $sort: { $field: 'name', $order: 'asc' },
//         $limit: 10,
//       },
//     },
//   })
//
//   console.log(c)
//   // t.deepEqual(
//   //   c,
//   //   {
//   //     children: [
//   //       { value: 0, name: 'Flurp999' },
//   //       { value: 0, name: 'flurp0' },
//   //       { value: 1, name: 'flurp1' },
//   //       { value: 2, name: 'flurp2' },
//   //       { value: 3, name: 'flurp3' },
//   //       { value: 4, name: 'flurp4' },
//   //       { value: 5, name: 'flurp5' },
//   //       { value: 6, name: 'flurp6' },
//   //       { value: 7, name: 'flurp7' },
//   //       { value: 8, name: 'flurp8' },
//   //       { value: 9, name: 'flurp9' },
//   //     ],
//   //   },
//   //   'non redis search sort'
//   // )
//
//   await client.destroy()
// })

test.serial('get - simple $list with $field option', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      custom: {
        prefix: 'cu',
        fields: {
          value: { type: 'number', search: true },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
    },
  })

  const children = []

  for (let i = 0; i < 100; i++) {
    const opts: any = {
      type: 'custom',
      value: i,
      name: 'flurp' + i,
    }

    if (i % 2) {
      opts.image = { thumb: 'flurp' + i }
    }

    children.push(opts)
  }

  await Promise.all([
    client.set({
      $id: 'cuA',
      image: {
        thumb: 'flurp.jpg',
      },
      title: { en: 'snurf' },
      children,
    }),
  ])

  const c = await client.get({
    $id: 'cuA',
    children: {
      title: { $field: 'name' },
      value: true,
      image: { thumb: { $default: 'default_image.jpg' } },
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $limit: 10,
      },
    },
  })

  t.deepEqual(
    c,
    {
      children: [
        { value: 0, title: 'flurp0', image: { thumb: 'default_image.jpg' } },
        { value: 1, title: 'flurp1', image: { thumb: 'flurp1' } },
        { value: 2, title: 'flurp2', image: { thumb: 'default_image.jpg' } },
        { value: 3, title: 'flurp3', image: { thumb: 'flurp3' } },
        { value: 4, title: 'flurp4', image: { thumb: 'default_image.jpg' } },
        { value: 5, title: 'flurp5', image: { thumb: 'flurp5' } },
        { value: 6, title: 'flurp6', image: { thumb: 'default_image.jpg' } },
        { value: 7, title: 'flurp7', image: { thumb: 'flurp7' } },
        { value: 8, title: 'flurp8', image: { thumb: 'default_image.jpg' } },
        { value: 9, title: 'flurp9', image: { thumb: 'flurp9' } },
      ],
    },
    'non redis search sort'
  )

  const { children: rangeResult } = await client.get({
    $id: 'cuA',
    children: {
      title: { $field: 'name' },
      value: true,
      $list: {
        $limit: 10,
      },
    },
  })

  t.is(rangeResult.length, 10, 'non redis search range')

  /*
  const x = await client.get({
    $id: 'cuA',
    related: {
      $inherit: true,
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $range: [0, 10]
      }
    }
  })
  */

  await client.destroy()
})
