import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import getPort from 'get-port'

import { wait } from './assertions/util'

let srv
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
})

test.beforeEach(async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'en_us', 'en_uk', 'de', 'nl'],
    rootType: {
      fields: {
        value: { type: 'number' }
      }
    },
    types: {
      club: {
        prefix: 'cl',
        fields: {
          specialMatch: { type: 'reference' },
          nested: {
            type: 'object',
            properties: {
              specialMatch: { type: 'reference' }
            }
          },
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
          value: { type: 'number' },
          title: { type: 'text' },
          description: { type: 'text' }
        }
      }
    }
  })

  await wait(500)
  //await new Promise(r => setTimeout(r, 30 * 1000))

  await client.destroy()
})

test.after(async t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('simple singular reference', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  // const match1 = await client.set({
  //   $id: 'maA',
  //   title: {
  //     en: 'yesh match'
  //   }
  // })

  // const club1 = await client.set({
  //   $id: 'clA',
  //   title: {
  //     en: 'yesh club'
  //   },
  //   specialMatch: match1
  // })

  const club1 = await client.set({
    $id: 'clA',
    title: {
      en: 'yesh club'
    },
    specialMatch: {
      $id: 'maA',
      title: {
        en: 'yesh match'
      }
    }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      title: true,
      specialMatch: true
    }),
    {
      title: 'yesh club',
      specialMatch: 'maA'
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      title: true,
      specialMatch: {
        title: true,
        description: { $default: 'no description' }
      }
    }),
    {
      title: 'yesh club',
      specialMatch: {
        title: 'yesh match',
        description: 'no description'
      }
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('simple singular reference with $flatten', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  const club1 = await client.set({
    $id: 'clA',
    title: {
      en: 'yesh club'
    },
    specialMatch: {
      $id: 'maA',
      title: {
        en: 'yesh match'
      }
    }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      id: true,
      title: true,
      specialMatch: {
        $flatten: true,
        title: true,
        description: { $default: 'no description' }
      }
    }),
    {
      id: 'clA',
      title: 'yesh match',
      description: 'no description'
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('nested singular reference with $flatten', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  const club1 = await client.set({
    $id: 'clA',
    title: {
      en: 'yesh club'
    },
    nested: {
      specialMatch: {
        $id: 'maA',
        title: {
          en: 'yesh match'
        }
      }
    }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      id: true,
      title: true,
      nested: {
        specialMatch: {
          $flatten: true,
          title: true,
          description: { $default: 'no description' }
        }
      }
    }),
    {
      id: 'clA',
      title: 'yesh club',
      nested: {
        title: 'yesh match',
        description: 'no description'
      }
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('singular reference inherit', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $id: 'maB',
    value: 112
  })

  const match1 = await client.set({
    $id: 'maA',
    title: {
      en: 'yesh match'
    },
    parents: {
      $add: 'maB'
    }
  })

  const club1 = await client.set({
    $id: 'clA',
    title: {
      en: 'yesh club'
    },
    specialMatch: match1
  })

  // const club1 = await client.set({
  //   $id: 'clA',
  //   title: {
  //     en: 'yesh club'
  //   },
  //   specialMatch: {
  //     $id: 'maA',
  //     title: {
  //       en: 'yesh match'
  //     }
  //   }
  // })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      title: true,
      specialMatch: {
        title: true,
        value: { $inherit: true }
      }
    }),
    {
      title: 'yesh club',
      specialMatch: {
        title: 'yesh match',
        value: 112
      }
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('singular reference $field', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  const match1 = await client.set({
    $id: 'maA',
    title: {
      en: 'yesh match'
    }
  })

  const club1 = await client.set({
    $id: 'clA',
    title: {
      en: 'yesh club'
    },
    specialMatch: match1
  })

  // const club1 = await client.set({
  //   $id: 'clA',
  //   title: {
  //     en: 'yesh club'
  //   },
  //   specialMatch: {
  //     $id: 'maA',
  //     title: {
  //       en: 'yesh match'
  //     }
  //   }
  // })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      title: true,
      match: {
        $field: 'specialMatch',
        title: true
      }
    }),
    {
      title: 'yesh club',
      match: {
        title: 'yesh match'
      }
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('singular reference inherit reference', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $id: 'clB',
    specialMatch: 'maA'
  })

  await client.set({
    $id: 'maB',
    value: 9001
  })

  const match1 = await client.set({
    $id: 'maA',
    title: {
      en: 'yesh match'
    },
    parents: {
      $add: 'maB'
    }
  })

  const club1 = await client.set({
    $id: 'clA',
    title: {
      en: 'yesh club'
    },
    parents: {
      $add: 'clB'
    }
  })

  // const club1 = await client.set({
  //   $id: 'clA',
  //   title: {
  //     en: 'yesh club'
  //   },
  //   specialMatch: {
  //     $id: 'maA',
  //     title: {
  //       en: 'yesh match'
  //     }
  //   }
  // })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      title: true,
      special: {
        $field: 'specialMatch',
        $inherit: true,
        title: true,
        value: { $inherit: true }
      }
    }),
    {
      title: 'yesh club',
      special: {
        title: 'yesh match',
        value: 9001
      }
    }
  )

  await client.delete('root')
  await client.destroy()
})
