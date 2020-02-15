import test from 'ava'
import { connect } from '../client/src/index'
import { start } from '../server/src/index'
import './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })

  const client = connect({ port }, { loglevel: 'info' })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        value: { type: 'number' },
        nested: {
          type: 'object',
          properties: {
            fun: { type: 'string' }
          }
        }
      }
    },
    types: {
      lekkerType: {
        prefix: 'vi',
        fields: {
          thing: { type: 'set', items: { type: 'string' } },
          ding: {
            type: 'object',
            properties: {
              dong: { type: 'set', items: { type: 'string' } },
              dung: { type: 'number' },
              dang: {
                type: 'object',
                properties: {
                  dung: { type: 'number' }
                }
              }
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

test.after(async _t => {
  // const client = connect({ port })
  // await client.delete('root')
  // await client.destroy()
  // await srv.destroy()
})

test.serial('get $value', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'maTest',
    title: { en: 'hello' }
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
            complex: true
          }
        }
      }
    }),
    {
      id: 'maTest',
      someField: 'some value',
      title: 'overwrite title as string',
      objectField: {
        something: {
          complex: true
        }
      }
    }
  )

  await client.delete('root')
  client.destroy()
})

test.serial('get complex with $value and array syntax', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $id: 'maTest',
    title: { en: 'hello' },
    description: { en: 'yesh' }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maTest',
      allMyThings: [
        {
          id: true,
          parents: true,
          ancestors: true
        },
        {
          name: true,
          somethingNice: { $value: 'yesh' }
        },
        {
          title: true,
          description: true
        }
      ]
    }),
    {
      allMyThings: [
        {
          id: 'maTest',
          parents: ['root'],
          ancestors: ['root']
        },
        {
          name: '',
          somethingNice: 'yesh'
        },
        {
          title: { en: 'hello' },
          description: { en: 'yesh' }
        }
      ]
    }
  )

  await client.delete('root')
  client.destroy()
})

test.serial('get - root', async t => {
  const client = connect({ port })

  const match = await client.set({
    $id: 'maTest'
  })

  await client.set({
    $id: 'root',
    value: 2555
  })

  t.deepEqual(
    await client.get({
      $id: 'root',
      id: true,
      value: true,
      children: true
    }),
    {
      id: 'root',
      value: 2555,
      children: [match]
    }
  )

  await client.set({
    $id: 'root',
    nested: { fun: 'yes fun' }
  })

  t.deepEqual(
    await client.get({
      $id: 'root',
      id: true,
      nested: { $all: true }
    }),
    {
      id: 'root',
      nested: { fun: 'yes fun' }
    }
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - basic', async t => {
  const client = connect({ port })

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

test.serial('get - $all simple', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $id: 'maA',
    title: {
      en: 'nice!'
    },
    description: {
      en: 'yesh'
    }
  })

  t.deepEqual(
    await client.get({
      $id: 'maA',
      $all: true
    }),
    {
      id: 'maA',
      type: 'match',
      name: '',
      title: {
        en: 'nice!'
      },
      description: {
        en: 'yesh'
      }
    }
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - $all root level whitelist + $all', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'clA',
    title: {
      en: 'nice!'
    },
    description: {
      en: 'yesh'
    },
    image: {
      thumb: 'thumb',
      poster: 'poster'
    }
  })

  t.deepEqual(
    await client.get({
      $id: 'clA',
      image: {
        thumb: true
      },
      $all: true
    }),
    {
      id: 'clA',
      type: 'club',
      name: '',
      title: {
        en: 'nice!'
      },
      description: {
        en: 'yesh'
      },
      image: {
        thumb: 'thumb'
      }
    }
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - $all root level whitelist + blacklists + $all', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'clA',
    title: {
      en: 'nice!'
    },
    description: {
      en: 'yesh'
    },
    image: {
      thumb: 'thumb',
      poster: 'poster'
    }
  })

  t.deepEqual(
    await client.get({
      $id: 'clA',
      image: {
        $all: true,
        thumb: true,
        poster: false
      },
      description: false,
      $all: true
    }),
    {
      id: 'clA',
      type: 'club',
      name: '',
      title: {
        en: 'nice!'
      },
      image: {
        thumb: 'thumb'
      }
    }
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - $all nested', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'maA',
    title: {
      en: 'nice!'
    },
    description: {
      en: 'yesh'
    }
  })

  t.deepEqual(
    await client.get({
      $id: 'maA',
      id: true,
      title: {
        $all: true
      },
      description: {
        $all: true
      }
    }),
    {
      id: 'maA',
      title: {
        en: 'nice!',
        de: '',
        nl: ''
      },
      description: {
        en: 'yesh',
        de: '',
        nl: ''
      }
    }
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - $all deeply nested', async t => {
  const client = connect({ port })

  const entry = await client.set({
    type: 'lekkerType',
    title: {
      en: 'nice!'
    },
    ding: {
      dang: {
        dung: 115
      }
    }
  })

  t.deepEqual(
    await client.get({
      $id: entry,
      id: true,
      title: {
        en: true
      },
      ding: { $all: true }
    }),
    {
      id: entry,
      title: {
        en: 'nice!'
      },
      ding: {
        dang: {
          dung: 115
        },
        dong: []
      }
    }
  )

  t.deepEqual(
    await client.get({
      $id: entry,
      id: true,
      title: {
        en: true
      },
      ding: { dang: { $all: true } }
    }),
    {
      id: entry,
      title: {
        en: 'nice!'
      },
      ding: {
        dang: {
          dung: 115
        }
      }
    }
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - $default', async t => {
  const client = connect({ port })

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
  const client = connect({ port })
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
  const client = connect({ port })

  const id = await client.set({
    type: 'lekkerType',
    thing: [],
    dong: { dingdong: [] },
    ding: { dong: [] },
    dingdongs: [],
    refs: []
  })

  const result = await client.get({
    $id: id,
    thing: true,
    dong: true,
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
    ding: { dong: [] },
    dong: { dingdong: [] }
  })

  client.destroy()
})

test.serial('get - hierarchy', async t => {
  const client = connect({ port })

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
        // image: { thumb: 'flurp.jpg' }, // image is not required
        id: 'cuB'
      }
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuC',
      flapdrol: {
        $inherit: { $item: ['custom', 'club'], $required: ['image'] },
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
        $inherit: { $type: ['custom', 'club'], $required: ['image.thumb'] }
      }
    }),
    {
      image: { thumb: 'flurp.jpg' }
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'cuC',
      id: true,
      flapdrol: {
        $inherit: { $item: ['custom', 'club'], $required: ['image.icon'] },
        image: true,
        id: true
      }
    }),
    {
      id: 'cuC',
      flapdrol: {}
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

test.serial.only(
  'get - $inherit with object types does shallow merge',
  async t => {
    const client = connect({ port }, { loglevel: 'info' })

    const parentOfParent = await client.set({
      type: 'lekkerType',
      title: {
        en: 'nice!',
        de: 'dont want to inherit this'
      },
      ding: {
        dang: {
          dung: 9000
        },
        dong: ['hello', 'yesh'],
        dung: 123
      }
    })

    const parentEntry = await client.set({
      type: 'lekkerType',
      title: {
        en: 'nice!',
        de: 'dont want to inherit this'
      },
      parents: {
        $add: [parentOfParent]
      },
      ding: {
        dang: {
          dung: 115
        }
      }
    })

    const entry = await client.set({
      type: 'lekkerType',
      parents: {
        $add: [parentEntry]
      },
      title: {
        en: 'nice!'
      }
    })

    console.log(
      await client.get({
        $id: entry,
        id: true,
        title: { $inherit: true },
        ding: { $inherit: true }
      })
    )
    // t.deepEqual(
    t.deepEqualIgnoreOrder(
      await client.get({
        $id: entry,
        id: true,
        title: { $inherit: true },
        ding: { $inherit: true }
      }),
      {
        id: entry,
        title: {
          en: 'nice!'
        },
        ding: {
          dong: ['hello', 'yesh'],
          dang: {
            dung: 115
          },
          dung: 123
        }
      }
    )

    // await client.delete('root')

    client.destroy()
  }
)

// test.serial('get - $field (basic)', async t => {
//   const client = connect({ port })

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
//
