import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'

test.before(async t => {
  await start({
    port: 6072,
    developmentLogging: true,
    loglevel: 'info'
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })

  const client = connect({ port: 6072 })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      lekkerType: {
        prefix: 'vi',
        fields: {
          thing: { type: 'set', items: { type: 'string' } },
          ding: {
            type: 'object',
            properties: {
              dong: { type: 'set', items: { type: 'string' } }
            }
          },
          dong: { type: 'json' },
          dingdongs: { type: 'array', items: { type: 'string' } },
          refs: { type: 'references' },
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      custom: {
        prefix: 'cu',
        fields: {
          name: { type: 'string' },
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      club: {
        prefix: 'cl',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          description: { type: 'text' }
        }
      }
    }
  })

  await client.destroy()
})

test.serial('get - basic', async t => {
  const client = connect({ port: 6072 })

  await client.set({
    $id: 'viA',
    title: {
      en: 'nice!'
    },
    value: 25,
    auth: {
      // role needs to be different , different roles per scope should be possible
      role: {
        id: ['root'],
        type: 'admin'
      }
    }
  })

  t.deepEqual(
    await client.get({
      $id: 'viA',
      id: true,
      title: true,
      value: true
    }),
    {
      id: 'viA',
      title: { en: 'nice!' },
      value: 25
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'viA',
      auth: true
    }),
    {
      auth: { role: { id: ['root'], type: 'admin' } }
    },
    'get role'
  )

  t.deepEqual(
    await client.get({
      $id: 'viA',
      auth: { role: { id: true } }
    }),
    {
      auth: { role: { id: ['root'] } }
    },
    'get role nested'
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - $default', async t => {
  const client = connect({ port: 6072 })

  await client.set({
    $id: 'viflap',
    title: { en: 'flap' }
  })

  t.deepEqual(
    await client.get({
      $id: 'viflap',
      age: { $default: 100 }
    }),
    { age: 100 }
  )

  t.deepEqual(
    await client.get({
      $id: 'viflap',
      title: {
        en: { $default: 'untitled' },
        nl: { $default: 'naamloos' }
      }
    }),
    {
      title: { en: 'flap', nl: 'naamloos' }
    }
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - $language', async t => {
  const client = connect({ port: 6072 })
  await client.set({
    $id: 'viflap',
    title: { en: 'flap', nl: 'flurp' },
    description: { en: 'yes', nl: 'ja' }
  })

  t.deepEqual(
    await client.get({
      $id: 'viflap',
      title: true,
      description: true,
      $language: 'nl'
    }),
    {
      title: 'flurp',
      description: 'ja'
    }
  )

  await client.set({
    $id: 'viflurx',
    title: { en: 'flap', nl: 'flurp' }
  })

  t.deepEqual(
    await client.get({
      $id: 'viflurx',
      $language: 'nl',
      description: { $default: 'flurpy' }
    }),
    { description: 'flurpy' }
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - field with empty array', async t => {
  const client = connect({ port: 6072 })

  const id = await client.set({
    type: 'lekkerType',
    thing: [],
    ding: { dong: [] },
    dingdongs: [],
    refs: []
  })

  const result = await client.get({
    $id: id,
    thing: true,
    // dong: true, // FIXME
    ding: { dong: true },
    dingdongs: true,
    children: true,
    descendants: true,
    refs: true
  })

  t.deepEqual(result, {
    thing: [],
    children: [],
    descendants: [],
    dingdongs: [],
    refs: [],
    ding: { dong: [] }
    //    dong: [],
  })

  client.destroy()
})

test.serial('get - hierarchy', async t => {
  const client = connect({ port: 6072 })

  await Promise.all([
    client.set({
      $id: 'viflapx',
      children: ['vifla', 'viflo']
    }),
    client.set({
      $id: 'vifla',
      children: ['viflo', 'maflux']
    })
  ])

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'viflapx',
      descendants: true,
      children: true,
      parents: true
    }),
    {
      descendants: ['viflo', 'vifla', 'maflux'],
      children: ['viflo', 'vifla'],
      parents: ['root']
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maflux',
      ancestors: true
    }),
    {
      ancestors: ['root', 'vifla', 'viflapx']
    }
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - $inherit', async t => {
  const client = connect({ port: 6072 })

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
        thumb: 'flurp.jpg'
      },
      title: { en: 'snurf' },
      children: ['cuB', 'cuC']
    }),
    client.set({
      $id: 'cuB',
      children: ['cuC', 'cuD']
    }),
    client.set({
      $id: 'cuX',
      children: ['cuC']
    }),
    client.set({
      $id: 'clClub',
      image: {
        thumb: 'bla.jpg'
      },
      children: ['cuB']
    }),
    client.set({
      $id: 'cuDfp',
      name: 'dfp',
      image: {
        thumb: 'dfp.jpg'
      },
      children: ['cuD']
    }),
    client.set({
      $id: 'cuMrsnurfels',
      name: 'MrSnurfels',
      image: {
        thumb: 'snurfels.jpg'
      },
      children: ['cuD']
    })
  ])

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuD',
      title: { $inherit: true }
    }),
    {
      title: {
        en: 'snurf'
      }
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuC',
      $language: 'nl',
      title: { $inherit: true }
    }),
    {
      title: 'snurf'
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuC',
      club: {
        $inherit: { $item: 'club' },
        image: true,
        id: true
      }
    }),
    {
      club: {
        image: { thumb: 'bla.jpg' },
        id: 'clClub'
      }
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuC',
      flapdrol: {
        $inherit: { $item: ['custom', 'club'] },
        image: true,
        id: true
      }
    }),
    {
      flapdrol: {
        image: { thumb: 'flurp.jpg' },
        id: 'cuA'
      }
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuC',
      flapdrol: {
        $inherit: { $item: ['region', 'federation'] },
        image: true,
        id: true
      }
    }),
    {
      flapdrol: {}
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuC',
      image: {
        $inherit: { $type: ['custom', 'club'] }
      }
    }),
    {
      image: { thumb: 'flurp.jpg' }
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

  client.destroy()
})

// test.serial('get - $field (basic)', async t => {
//   const client = connect({ port: 6072 })

//   await client.set({
//     $id: 'reDe',
//     layout: {
//       match: { components: [{ type: 'list', blurf: true }] }
//     }
//   })

//   await client.set({
//     $id: 'maA',
//     parents: ['reDe']
//   })

//   await client.get({
//     $id: 'maA',
//     layout: {
//       $inherit: true,
//       $field: ['layout.$type', 'layout.default']
//     }
//   })
// })

// ADD FIELD
// ADD REF <-- ref mucho importante
