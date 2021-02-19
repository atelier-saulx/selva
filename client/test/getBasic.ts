import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })
})

test.beforeEach(async (t) => {
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        value: { type: 'number' },
        nested: {
          type: 'object',
          properties: {
            fun: { type: 'string' },
          },
        },
      },
    },
    types: {
      lekkerType: {
        prefix: 'vi',
        fields: {
          strRec: {
            type: 'record',
            values: {
              type: 'string',
            },
          },
          textRec: {
            type: 'record',
            values: {
              type: 'text',
            },
          },
          objRec: {
            type: 'record',
            values: {
              type: 'object',
              properties: {
                hello: {
                  type: 'string',
                },
                value: {
                  type: 'number',
                },
                stringValue: {
                  type: 'string',
                },
              },
            },
          },
          thing: { type: 'set', items: { type: 'string' } },
          ding: {
            type: 'object',
            properties: {
              dong: { type: 'set', items: { type: 'string' } },
              texty: { type: 'text' },
              dung: { type: 'number' },
              dang: {
                type: 'object',
                properties: {
                  dung: { type: 'number' },
                  dunk: { type: 'string' },
                },
              },
              dunk: {
                type: 'object',
                properties: {
                  ding: { type: 'number' },
                  dong: { type: 'number' },
                },
              },
            },
          },
          dong: { type: 'json' },
          dingdongs: { type: 'array', items: { type: 'string' } },
          refs: { type: 'references' },
          value: { type: 'number' },
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
      custom: {
        prefix: 'cu',
        fields: {
          value: { type: 'number' },
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
      club: {
        prefix: 'cl',
        fields: {
          value: { type: 'number' },
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
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          value: { type: 'number' },
          description: { type: 'text' },
        },
      },
      yesno: {
        prefix: 'yn',
        fields: {
          bolYes: { type: 'boolean' },
          bolNo: { type: 'boolean' },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('get $value', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'maTest',
    title: { en: 'hello' },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maTest',
      id: true,
      someField: { $value: 'some value' },
      title: { $value: 'overwrite title as string' },
      objectField: {
        $value: {
          something: {
            complex: true,
          },
        },
      },
    }),
    {
      id: 'maTest',
      someField: 'some value',
      title: 'overwrite title as string',
      objectField: {
        something: {
          complex: true,
        },
      },
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('get nested queries', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'maTest',
    value: 11,
    title: { en: 'hello' },
  })

  await client.set({
    $id: 'maTest2',
    value: 12,
    title: { en: 'halloumi' },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maTest',
      id: true,
      someItem: {
        $id: 'maTest2',
        title: true,
        nestedThing: { $id: 'maTest', value: true },
      },
      values: [
        {
          $id: 'maTest',
          id: true,
          value: true,
        },
        {
          $id: 'maTest2',
          id: true,
          value: true,
        },
      ],
      title: true,
    }),
    {
      id: 'maTest',
      title: { en: 'hello' },
      someItem: {
        title: {
          en: 'halloumi',
        },
        nestedThing: {
          value: 11,
        },
      },
      values: [
        {
          id: 'maTest',
          value: 11,
        },
        {
          id: 'maTest2',
          value: 12,
        },
      ],
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('get boolean value', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'ynTest',
    bolYes: true,
    bolNo: false,
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'ynTest',
      id: true,
      bolYes: true,
      bolNo: true,
    }),
    {
      id: 'ynTest',
      bolYes: true,
      bolNo: false,
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('get complex with $value and array syntax', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'maTest',
    title: { en: 'hello' },
    description: { en: 'yesh' },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maTest',
      allMyThings: [
        {
          id: true,
          parents: true,
          ancestors: true,
        },
        {
          name: true,
          somethingNice: { $value: 'yesh' },
        },
        {
          title: true,
          description: true,
        },
      ],
    }),
    {
      allMyThings: [
        {
          id: 'maTest',
          parents: ['root'],
          ancestors: ['root'],
        },
        {
          somethingNice: 'yesh',
        },
        {
          title: { en: 'hello' },
          description: { en: 'yesh' },
        },
      ],
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('get - root', async (t) => {
  const client = connect({ port })

  const match = await client.set({
    $id: 'maTest',
  })

  await client.set({
    $id: 'root',
    value: 2555,
  })

  t.deepEqual(
    await client.get({
      $id: 'root',
      id: true,
      value: true,
      children: true,
    }),
    {
      id: 'root',
      value: 2555,
      children: [match],
    }
  )

  t.deepEqual(
    await client.get({
      id: true,
      value: true,
      children: true,
    }),
    {
      id: 'root',
      value: 2555,
      children: [match],
    }
  )

  await client.set({
    $id: 'root',
    nested: { fun: 'yes fun' },
  })

  t.deepEqual(
    await client.get({
      $id: 'root',
      id: true,
      nested: { $all: true },
    }),
    {
      id: 'root',
      nested: { fun: 'yes fun' },
    }
  )

  await client.delete('root')

  await client.destroy()
})

test.serial('get - basic', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'viA',
    title: {
      en: 'nice!',
    },
    value: 25,
    auth: {
      // role needs to be different , different roles per scope should be possible
      role: {
        id: ['root'],
        type: 'admin',
      },
    },
  })

  t.deepEqual(
    await client.get({
      $id: 'viA',
      id: true,
      title: true,
      value: true,
    }),
    {
      id: 'viA',
      title: { en: 'nice!' },
      value: 25,
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'viA',
      auth: true,
    }),
    {
      auth: { role: { id: ['root'], type: 'admin' } },
    },
    'get role'
  )

  // not supported without 'properties'
  // t.deepEqual(
  //   await client.get({
  //     $id: 'viA',
  //     auth: { role: { id: true } }
  //   }),
  //   {
  //     auth: { role: { id: ['root'] } }
  //   },
  //   'get role nested'
  // )

  await client.delete('root')

  await client.destroy()
})

test.serial('get - $all simple', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'maA',
    title: {
      en: 'nice!',
    },
    description: {
      en: 'yesh',
    },
  })

  t.deepEqual(
    await client.get({
      $id: 'maA',
      $all: true,
      aliases: false,
    }),
    {
      id: 'maA',
      type: 'match',
      title: {
        en: 'nice!',
      },
      description: {
        en: 'yesh',
      },
    }
  )

  await client.delete('root')

  await client.destroy()
})

test.serial('get - $all root level whitelist + $all', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'clA',
    title: {
      en: 'nice!',
    },
    description: {
      en: 'yesh',
    },
    image: {
      thumb: 'thumb',
      poster: 'poster',
    },
  })

  t.deepEqual(
    await client.get({
      $id: 'clA',
      image: {
        thumb: true,
      },
      $all: true,
      aliases: false,
    }),
    {
      id: 'clA',
      type: 'club',
      title: {
        en: 'nice!',
      },
      description: {
        en: 'yesh',
      },
      image: {
        thumb: 'thumb',
      },
    }
  )

  await client.delete('root')

  await client.destroy()
})

test.serial(
  'get - $all root level whitelist + blacklists + $all',
  async (t) => {
    const client = connect({ port })

    await client.set({
      $id: 'clA',
      title: {
        en: 'nice!',
      },
      description: {
        en: 'yesh',
      },
      image: {
        thumb: 'thumb',
        poster: 'poster',
      },
    })

    t.deepEqual(
      await client.get({
        $id: 'clA',
        image: {
          $all: true,
          thumb: true,
          poster: false,
        },
        description: false,
        $all: true,
        aliases: false,
      }),
      {
        id: 'clA',
        type: 'club',
        title: {
          en: 'nice!',
        },
        image: {
          thumb: 'thumb',
        },
      }
    )

    await client.delete('root')

    await client.destroy()
  }
)

test.serial('get - $all nested', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'maA',
    title: {
      en: 'nice!',
    },
    description: {
      en: 'yesh',
    },
  })

  t.deepEqual(
    await client.get({
      $id: 'maA',
      id: true,
      title: {
        $all: true,
      },
      description: {
        $all: true,
      },
    }),
    {
      id: 'maA',
      title: {
        en: 'nice!',
      },
      description: {
        en: 'yesh',
      },
    }
  )

  await client.delete('root')

  await client.destroy()
})

test.serial('get - $all deeply nested', async (t) => {
  const client = connect({ port })

  const entry = await client.set({
    type: 'lekkerType',
    title: {
      en: 'nice!',
    },
    ding: {
      dang: {
        dung: 115,
        dunk: '',
      },
    },
  })

  t.deepEqual(
    await client.get({
      $id: entry,
      id: true,
      title: {
        en: true,
      },
      ding: { $all: true },
    }),
    {
      id: entry,
      title: {
        en: 'nice!',
      },
      ding: {
        dang: {
          dung: 115,
          dunk: '',
        },
      },
    }
  )

  t.deepEqual(
    await client.get({
      $id: entry,
      id: true,
      title: {
        en: true,
      },
      ding: { dang: { $all: true } },
    }),
    {
      id: entry,
      title: {
        en: 'nice!',
      },
      ding: {
        dang: {
          dung: 115,
          dunk: '',
        },
      },
    }
  )

  await client.delete('root')

  await client.destroy()
})

test.serial('get - $default', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'viflap',
    title: { en: 'flap' },
  })

  t.deepEqual(
    await client.get({
      $id: 'viflap',
      age: { $default: 100 },
    }),
    { age: 100 }
  )

  t.deepEqual(
    await client.get({
      $id: 'viflap',
      title: {
        en: { $default: 'untitled' },
        nl: { $default: 'naamloos' },
      },
    }),
    {
      title: { en: 'flap', nl: 'naamloos' },
    }
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - $language', async (t) => {
  const client = connect({ port })
  await client.set({
    $id: 'viflap',
    title: { en: 'flap', nl: 'flurp' },
    description: { en: 'yes', nl: 'ja' },
  })

  t.deepEqual(
    await client.get({
      $id: 'viflap',
      title: true,
      description: true,
      $language: 'nl',
    }),
    {
      title: 'flurp',
      description: 'ja',
    }
  )

  await client.set({
    $id: 'viflurx',
    title: { en: 'flap', nl: 'flurp' },
  })

  t.deepEqual(
    await client.get({
      $id: 'viflurx',
      $language: 'nl',
      description: { $default: 'flurpy' },
    }),
    { description: 'flurpy' }
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - field with empty array', async (t) => {
  const client = connect({ port })

  const id = await client.set({
    type: 'lekkerType',
    thing: [],
    dong: { dingdong: [] },
    ding: { dong: [] },
    dingdongs: [],
    refs: [],
  })

  const result = await client.get({
    $id: id,
    thing: true,
    dong: true,
    ding: { dong: true },
    dingdongs: true,
    children: true,
    descendants: true,
    refs: true,
  })

  t.deepEqual(result, {
    children: [],
    descendants: [],
    dingdongs: [],
    dong: { dingdong: [] },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: id,
      $all: true,
    }),
    {
      id,
      dong: { dingdong: [] },
      type: 'lekkerType',
      dingdongs: [],
    }
  )

  client.destroy()
})

test.serial('get - set with some items', async (t) => {
  const client = connect({ port })

  const id = await client.set({
    type: 'lekkerType',
    thing: ['a', 'b'],
  })

  const result = await client.get({
    $id: id,
    thing: true,
  })

  t.deepEqual(result, {
    thing: ['a', 'b'],
  })

  client.destroy()
})

test.serial('get - hierarchy', async (t) => {
  const client = connect({ port })

  await Promise.all([
    await client.set({
      $id: 'viflo',
    }),
    await client.set({
      $id: 'maflux',
    }),
  ])
  await client.set({
    $id: 'vifla',
    children: ['viflo', 'maflux'],
  })
  await client.set({
    $id: 'viflapx',
    children: ['vifla', 'viflo'],
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'viflapx',
      descendants: true,
      children: true,
      parents: true,
    }),
    {
      descendants: ['viflo', 'vifla', 'maflux'],
      children: ['viflo', 'vifla'],
      parents: ['root'],
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maflux',
      ancestors: true,
    }),
    {
      ancestors: ['root', 'vifla', 'viflapx'],
    }
  )

  await client.delete('root')

  await client.destroy()
})

test.serial('get - $inherit', async (t) => {
  const client = connect({ port })
  /*
    root
      |_ cuX
          |_cuC
      |_cuA
          |_cuC
          |_cuB
              |_cuD <---
                |_cuC

      |_cuFlap
        |_cuFlurp
          |_cuD <---


      |_clClub
        |_cuB
      |_cuDfp
        |_cuD
      |_cuMrsnurfels
        |_cuD


      root
      |_ leA
          |_seasonA
             |_matchA
             |_teamA //ignoe ancestor from cut of point outside of my hierarchy
                |_matchA

      |_ leB
          |_seasonB
             |_teamA
                |_matchA

  */

  // close ancestors [ cuMrsnurfels, cuDfp, cuB, clClub, root ]

  // close ancestors of cuD
  // dfp, cub, cuD

  await Promise.all([
    client.set({
      $id: 'cuA',
      image: {
        thumb: 'flurp.jpg',
      },
      title: { en: 'snurf' },
      children: ['cuB', 'cuC'],
    }),
    client.set({
      $id: 'cuB',
      children: ['cuC', 'cuD'],
    }),
    client.set({
      $id: 'cuX',
      children: ['cuC'],
    }),
    client.set({
      $id: 'clClub',
      image: {
        thumb: 'bla.jpg',
      },
      children: ['cuB'],
    }),
    client.set({
      $id: 'cuDfp',
      name: 'dfp',
      image: {
        thumb: 'dfp.jpg',
      },
      children: ['cuD'],
    }),
    client.set({
      $id: 'cuMrsnurfels',
      name: 'MrSnurfels',
      image: {
        thumb: 'snurfels.jpg',
      },
      children: ['cuD'],
    }),
  ])

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuD',
      title: { $inherit: { $type: ['custom', 'club'] } },
    }),
    {
      title: {
        en: 'snurf',
      },
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuC',
      $language: 'nl',
      title: { $inherit: { $type: ['custom', 'club'] } },
    }),
    {
      title: 'snurf',
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuC',
      club: {
        $inherit: { $item: 'club' },
        image: true,
        id: true,
      },
    }),
    {
      club: {
        image: { thumb: 'bla.jpg' },
        id: 'clClub',
      },
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuC',
      flapdrol: {
        $inherit: { $item: ['custom', 'club'] },
        image: true,
        id: true,
      },
    }),
    {
      flapdrol: {
        image: { thumb: 'flurp.jpg' },
        id: 'cuA',
      },
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuC',
      flapdrol: {
        $inherit: { $item: ['custom', 'club'], $required: ['image'] },
        image: true,
        id: true,
      },
    }),
    {
      flapdrol: {
        image: { thumb: 'flurp.jpg' },
        id: 'cuA',
      },
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuC',
      flapdrol: {
        $inherit: { $item: ['region', 'federation'] },
        image: true,
        id: true,
      },
    }),
    {
      // flapdrol: {}
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuC',
      image: {
        $inherit: { $type: ['custom', 'club'] },
      },
    }),
    {
      image: { thumb: 'flurp.jpg' },
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuC',
      id: true,
      flapdrol: {
        $inherit: { $item: ['custom', 'club'], $required: ['image.icon'] },
        image: true,
        id: true,
      },
    }),
    {
      id: 'cuC',
      // flapdrol: {}
    }
  )

  // FIXME: is the order really specific here?
  // t.deepEqualIgnoreOrder(
  //   await client.get({
  //     $id: 'cuD',
  //     image: {
  //       $inherit: { $name: ['dfp', 'MrSnurfels'] }
  //     }
  //   }),
  //   {
  //     image: { thumb: 'dfp.jpg' }
  //   }
  // )

  await client.delete('root')

  await client.destroy()
})

test.serial(
  'get - $inherit with object types does shallow merge',
  async (t) => {
    const client = connect({ port })

    const parentOfParent = await client.set({
      $id: 'vipofp',
      type: 'lekkerType',
      title: {
        en: 'nice!',
        de: 'dont want to inherit this',
      },
      ding: {
        dang: {
          dung: 9000,
          dunk: 'helloooo should not be there',
        },
        dong: ['hello', 'yesh'],
        dung: 123,
      },
    })

    const parentEntry = await client.set({
      $id: 'vip',
      type: 'lekkerType',
      title: {
        en: 'nice!',
        de: 'dont want to inherit this',
      },
      parents: {
        $add: [parentOfParent],
      },
      ding: {
        dang: {
          dung: 115,
        },
      },
    })

    const entry = await client.set({
      $id: 'vie',
      type: 'lekkerType',
      parents: {
        $add: [parentEntry],
      },
      title: {
        en: 'nice!',
      },
    })

    t.deepEqualIgnoreOrder(
      await client.get({
        $id: entry,
        id: true,
        title: { $inherit: { $merge: true } },
        ding: { $inherit: { $merge: true } },
      }),
      {
        id: entry,
        title: {
          en: 'nice!',
        },
        ding: {
          dong: ['hello', 'yesh'],
          dang: {
            dung: 115,
          },
          dung: 123,
        },
      }
    )

    await client.delete('root')

    client.destroy()
  }
)

test.serial(
  'get - $inherit with object types shallow merge is by default disabled',
  async (t) => {
    const client = connect({ port })

    const parentOfParent = await client.set({
      type: 'lekkerType',
      title: {
        en: 'nice!',
        de: 'dont want to inherit this',
      },
      ding: {
        dang: {
          dung: 9000,
          dunk: 'helloooo should not be there',
        },
        dong: ['hello', 'yesh'],
        dung: 123,
      },
    })

    const parentEntry = await client.set({
      type: 'lekkerType',
      title: {
        en: 'nice!',
        de: 'dont want to inherit this',
      },
      parents: {
        $add: [parentOfParent],
      },
      ding: {
        dang: {
          dung: 115,
        },
      },
    })

    const entry = await client.set({
      type: 'lekkerType',
      parents: {
        $add: [parentEntry],
      },
      title: {
        en: 'nice!',
      },
    })

    t.deepEqualIgnoreOrder(
      await client.get({
        $id: entry,
        id: true,
        title: { $inherit: { $type: 'lekkerType' } },
        ding: { $inherit: { $type: ['lekkerType'] } },
      }),
      {
        id: entry,
        title: {
          en: 'nice!',
        },
        ding: {
          dang: {
            dung: 115,
          },
        },
      }
    )

    await client.delete('root')

    await client.destroy()
  }
)

test.serial(
  'get - $inherit with object types of nested objects, does shallow merge',
  async (t) => {
    const client = connect({ port })

    const parentOfParent = await client.set({
      type: 'lekkerType',
      title: {
        en: 'nice!',
        de: 'dont want to inherit this',
      },
      ding: {
        dang: {
          dung: 9000,
          dunk: 'yesh',
        },
        dunk: {
          ding: 9000,
          dong: 9000,
        },
        dung: 123,
      },
    })

    const parentEntry = await client.set({
      type: 'lekkerType',
      title: {
        en: 'nice!',
        de: 'dont want to inherit this',
      },
      parents: {
        $add: [parentOfParent],
      },
      ding: {
        dang: {
          dung: 115,
        },
        dunk: {
          ding: 123,
        },
      },
    })

    const entry = await client.set({
      type: 'lekkerType',
      parents: {
        $add: [parentEntry],
      },
      title: {
        en: 'nice!',
      },
      ding: {
        dung: 1,
      },
    })

    t.deepEqualIgnoreOrder(
      await client.get({
        $id: entry,
        id: true,
        title: { $inherit: { $type: 'lekkerType' } },
        ding: {
          dang: { $inherit: { $type: 'lekkerType', $merge: true } },
          dunk: { $inherit: { $type: 'lekkerType', $merge: true } },
          dung: { $inherit: { $type: 'lekkerType' } },
        },
      }),
      {
        id: entry,
        title: {
          en: 'nice!',
        },
        ding: {
          dang: {
            dung: 115,
            dunk: 'yesh',
          },
          dunk: {
            ding: 123,
            dong: 9000,
          },
          dung: 1,
        },
      }
    )

    await client.delete('root')

    await client.destroy()
  }
)

test.serial('get - basic with many ids', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'viA',
    title: {
      en: 'nice!',
    },
    value: 25,
    auth: {
      // role needs to be different , different roles per scope should be possible
      role: {
        id: ['root'],
        type: 'admin',
      },
    },
  })

  t.deepEqual(
    await client.get({
      $id: ['viZ', 'viA'],
      id: true,
      title: true,
      value: true,
    }),
    {
      id: 'viA',
      title: { en: 'nice!' },
      value: 25,
    }
  )

  t.deepEqual(
    await client.get({
      $id: ['viA', 'viZ'],
      value: true,
    }),
    {
      value: 25,
    }
  )

  t.deepEqual(
    await client.get({
      $alias: ['abba', 'viA'],
      value: true,
    }),
    {
      value: 25,
    }
  )

  await client.set({
    $id: 'viA',
    aliases: { $add: 'abba' },
  })

  t.deepEqual(
    await client.get({
      $alias: ['abba', 'viZ'],
      value: true,
    }),
    {
      value: 25,
    }
  )

  t.deepEqual(
    await client.get({
      $id: ['viZ', 'viY'],
      $language: 'en',
      id: true,
      title: true,
    }),
    {
      $isNull: true,
    }
  )

  await client.delete('root')

  await client.destroy()
})

test.serial('get - basic with non-priority language', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'viA',
    title: {
      de: 'nice de!',
    },
    value: 25,
    auth: {
      // role needs to be different , different roles per scope should be possible
      role: {
        id: ['root'],
        type: 'admin',
      },
    },
  })

  t.deepEqual(
    await client.get({
      $language: 'en',
      $id: ['viZ', 'viA'],
      id: true,
      title: true,
      value: true,
    }),
    {
      id: 'viA',
      title: 'nice de!',
      value: 25,
    }
  )

  t.deepEqual(
    await client.get({
      $language: 'nl',
      $id: ['viZ', 'viA'],
      id: true,
      title: true,
      value: true,
    }),
    {
      id: 'viA',
      title: 'nice de!',
      value: 25,
    }
  )

  await client.set({
    $id: 'viA',
    title: {
      nl: 'nice nl!',
    },
  })

  t.deepEqual(
    await client.get({
      $language: 'en',
      $id: ['viZ', 'viA'],
      id: true,
      title: true,
      value: true,
    }),
    {
      id: 'viA',
      title: 'nice de!',
      value: 25,
    }
  )

  t.deepEqual(
    await client.get({
      $language: 'nl',
      $id: ['viZ', 'viA'],
      id: true,
      title: true,
      value: true,
    }),
    {
      id: 'viA',
      title: 'nice nl!',
      value: 25,
    }
  )

  await client.delete('root')

  await client.destroy()
})

test.serial('get - record', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'viA',
    title: {
      en: 'nice!',
    },
    strRec: {
      hello: 'hallo',
      world: 'hmm',
    },
    objRec: {
      myObj1: {
        hello: 'pff',
        value: 12,
      },
      obj2: {
        hello: 'ffp',
        value: 12,
      },
    },
  })

  t.deepEqual(
    await client.get({
      $id: 'viA',
      $language: 'en',
      id: true,
      title: true,
      strRec: true,
    }),
    {
      id: 'viA',
      title: 'nice!',
      strRec: {
        hello: 'hallo',
        world: 'hmm',
      },
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'viA',
      $language: 'en',
      id: true,
      title: true,
      strRec: {
        world: true,
      },
    }),
    {
      id: 'viA',
      title: 'nice!',
      strRec: {
        world: 'hmm',
      },
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'viA',
      $language: 'en',
      id: true,
      title: true,
      objRec: true,
    }),
    {
      id: 'viA',
      title: 'nice!',
      objRec: {
        myObj1: {
          hello: 'pff',
          value: 12,
        },
        obj2: {
          hello: 'ffp',
          value: 12,
        },
      },
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'viA',
      $language: 'en',
      id: true,
      title: true,
      objRec: {
        myObj1: {
          value: true,
        },
        obj2: {
          hello: true,
        },
      },
    }),
    {
      id: 'viA',
      title: 'nice!',
      objRec: {
        myObj1: {
          value: 12,
        },
        obj2: {
          hello: 'ffp',
        },
      },
    }
  )

  await client.delete('root')

  await client.destroy()
})

test.serial('get - text record', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'viA',
    title: {
      en: 'nice!',
    },
    textRec: {
      hello: { en: 'hallo' },
      world: { en: 'hmm' },
    },
  })

  await client.set({
    $id: 'viB',
    title: {
      en: 'nice!',
    },
    textRec: {
      yes: { en: 'yes have it' },
    },
  })

  await client.set({
    $id: 'viC',
    title: {
      en: 'nice!',
    },
    parents: ['viB'],
  })

  t.deepEqual(
    await client.get({
      $id: 'viA',
      $language: 'en',
      id: true,
      title: true,
      textRec: true,
    }),
    {
      id: 'viA',
      title: 'nice!',
      textRec: {
        hello: 'hallo',
        world: 'hmm',
      },
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'viA',
      $language: 'en',
      id: true,
      title: true,
      textRec: {
        world: true,
      },
    }),
    {
      id: 'viA',
      title: 'nice!',
      textRec: {
        world: 'hmm',
      },
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'viC',
      $language: 'en',
      id: true,
      title: true,
      textRec: {
        $inherit: true,
      },
    }),
    {
      id: 'viC',
      title: 'nice!',
      textRec: {
        yes: 'yes have it',
      },
    }
  )

  await client.delete('root')

  await client.destroy()
})

test.serial('get - $inherit with object types does deep merge', async (t) => {
  const client = connect({ port })

  const parentOfParent = await client.set({
    $id: 'vipofp',
    type: 'lekkerType',
    title: {
      en: 'nice!',
      de: 'dont want to inherit this',
    },
    ding: {
      dang: {
        dung: 9000,
        dunk: 'hello this time it should be here',
      },
      dong: ['hello', 'yesh'],
      dung: 123,
    },
  })

  const parentEntry = await client.set({
    $id: 'vip',
    type: 'lekkerType',
    title: {
      en: 'nice!',
      de: 'dont want to inherit this',
    },
    parents: {
      $add: [parentOfParent],
    },
    ding: {
      texty: { de: 'hallo' },
      dang: {
        dung: 115,
      },
      dunk: {
        dong: 1212,
      },
    },
  })

  const entry = await client.set({
    $id: 'vie',
    type: 'lekkerType',
    parents: {
      $add: [parentEntry],
    },
    title: {
      en: 'nice!',
    },
    ding: {
      texty: { en: 'hello' },
      dunk: {
        ding: 99,
      },
    },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: entry,
      id: true,
      title: { $inherit: { $deepMerge: true } },
      ding: { $inherit: { $deepMerge: true } },
      // title: { $inherit: { $type: 'lekkerType', $merge: true } }, // TODO: throw, not allowed probably
      // ding: { $inherit: { $type: 'lekkerType', $merge: true } },
    }),
    {
      id: entry,
      title: {
        en: 'nice!',
      },
      ding: {
        texty: { en: 'hello' },
        dong: ['hello', 'yesh'],
        dang: {
          dung: 115,
          dunk: 'hello this time it should be here',
        },
        dung: 123,
        dunk: {
          ding: 99,
          dong: 1212,
        },
      },
    }
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - $inherit with record types does deep merge', async (t) => {
  const client = connect({ port })

  const parentOfParent = await client.set({
    $id: 'vipofp',
    type: 'lekkerType',
    title: {
      en: 'nice!',
      de: 'dont want to inherit this',
    },
    objRec: {
      a: {
        hello: 'not this one either',
        stringValue: 'yes string value',
      },
      b: {
        stringValue: 'inherit please',
      },
      c: {
        hello: 'yes hello from parentOfParent',
      },
      0: {
        hello: 'no',
        stringValue: 'also no',
        value: 99,
      },
    },
  })

  const parentEntry = await client.set({
    $id: 'vip',
    type: 'lekkerType',
    title: {
      en: 'nice!',
      de: 'dont want to inherit this',
    },
    parents: {
      $add: [parentOfParent],
    },
    objRec: {
      a: {
        hello: 'not this one',
        stringValue: 'this should be there',
      },
      b: {
        hello: 'yes hello from parent',
        value: 10,
      },
    },
  })

  const entry = await client.set({
    $id: 'vie',
    type: 'lekkerType',
    parents: {
      $add: [parentEntry],
    },
    title: {
      en: 'nice!',
    },
    objRec: {
      0: {
        hello: 'this is where it starts',
        stringValue: 'in the entry itself',
      },
    },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: entry,
      id: true,
      title: { $inherit: { $type: 'lekkerType', $deepMerge: true } }, // TODO: throw, not allowed probably
      objRec: { $inherit: { $type: 'lekkerType', $deepMerge: true } },
      // title: { $inherit: { $type: 'lekkerType', $merge: true } }, // TODO: throw, not allowed probably
      // objRec: { $inherit: { $type: 'lekkerType', $merge: true } },
    }),
    {
      id: entry,
      title: {
        en: 'nice!',
      },
      objRec: {
        0: {
          hello: 'this is where it starts',
          stringValue: 'in the entry itself',
          value: 99,
        },
        a: {
          hello: 'not this one',
          stringValue: 'this should be there',
        },
        b: {
          hello: 'yes hello from parent',
          value: 10,
          stringValue: 'inherit please',
        },
        c: {
          hello: 'yes hello from parentOfParent',
        },
      },
    }
  )

  await client.delete('root')

  client.destroy()
})
