import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'
import { deepCopy } from '@saulx/utils'

let srv
let port: number
test.before(async () => {
  port = await getPort()
  srv = await start({
    port
  })
})

test.after(async t => {
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('inherit object nested field from root youzi', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        flapper: {
          type: 'object',
          properties: {
            snurk: { type: 'json' },
            bob: { type: 'json' }
          }
        }
      }
    },
    types: {
      yeshType: {
        prefix: 'ye',
        fields: {
          flapper: {
            type: 'object',
            properties: {
              snurk: { type: 'json' },
              bob: { type: 'json' }
            }
          }
        }
      }
    }
  })

  await client.set({
    $id: 'root',
    flapper: {
      snurk: 'hello',
      bob: 'xxx'
    }
  })

  await client.set({
    $id: 'yeA'
  })

  const observable = client.observe({
    $id: 'yeA',
    flapper: { snurk: { $inherit: { $type: ['yeshType', 'root'] } } }
  })

  const results = []

  const subs = observable.subscribe(p => {
    // its now not immatable - think about if we want it immutable
    results.push(deepCopy(p))
  })

  await wait(2000)

  await client.set({
    $id: 'root',
    flapper: {
      snurk: 'snurkels'
    }
  })

  await wait(2000)

  subs.unsubscribe()

  t.deepEqual(results, [
    { flapper: { snurk: 'hello' } },
    { flapper: { snurk: 'snurkels' } }
  ])

  await client.delete('root')
  await client.destroy()
  t.true(true)
})

test.serial('inherit object youzi', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      yeshType: {
        prefix: 'ye',
        fields: {
          flapper: {
            type: 'object',
            properties: {
              snurk: { type: 'json' },
              bob: { type: 'json' }
            }
          }
        }
      }
    }
  })

  await client.set({
    $id: 'yeA',
    flapper: {
      snurk: 'hello',
      bob: 'xxx'
    }
  })

  const observable = client.observe({
    $id: 'yeA',
    flapper: { $inherit: { $type: 'yeshType' } }
  })

  const results = []

  const subs = observable.subscribe(p => {
    results.push(deepCopy(p))
  })

  await wait(1000)

  await client.set({
    $id: 'yeA',
    flapper: {
      snurk: 'snurkels'
    }
  })

  await wait(1000)

  subs.unsubscribe()

  t.deepEqual(results, [
    { flapper: { snurk: 'hello', bob: 'xxx' } },
    { flapper: { snurk: 'snurkels', bob: 'xxx' } }
  ])

  await client.delete('root')
  await client.destroy()
  t.true(true)
})

test.serial('basic inherit subscription', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        yesh: { type: 'string' },
        no: { type: 'string' },
        flapper: {
          type: 'object',
          properties: {
            snurk: { type: 'json' },
            bob: { type: 'json' }
          }
        }
      }
    },
    types: {
      yeshType: {
        prefix: 'ye',
        fields: {
          yesh: { type: 'string' },
          flapper: {
            type: 'object',
            properties: {
              snurk: { type: 'json' },
              bob: { type: 'json' }
            }
          }
        }
      }
    }
  })

  await client.set({
    $id: 'root',
    yesh: 'yesh',
    no: 'no'
  })

  await client.set({
    $id: 'yeA',
    yesh: 'yesh a'
  })

  await client.set({
    $id: 'yeB',
    parents: ['yeA']
  })

  const observable = client.observe({
    $id: 'yeB',
    yesh: { $inherit: { $type: ['yeshType', 'root'] } }
  })

  const results = []

  const subs = observable.subscribe(p => {
    results.push(deepCopy(p))
  })

  await wait(1000)

  await client.set({
    $id: 'yeA',
    yesh: 'yesh a!'
  })

  await wait(1000)

  await client.set({
    $id: 'yeB',
    yesh: 'yesh b'
  })

  await wait(1000)

  t.deepEqual(results, [
    { yesh: 'yesh a' },
    { yesh: 'yesh a!' },
    { yesh: 'yesh b' }
  ])

  subs.unsubscribe()

  await client.delete('root')
  await client.destroy()
  t.true(true)
})

test.serial('inherit object', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        flapper: {
          type: 'object',
          properties: {
            snurk: { type: 'json' },
            bob: { type: 'json' }
          }
        }
      }
    },
    types: {
      yeshType: {
        prefix: 'ye',
        fields: {
          flapper: {
            type: 'object',
            properties: {
              snurk: { type: 'json' },
              bob: { type: 'json' }
            }
          }
        }
      }
    }
  })

  await client.set({
    $id: 'root',
    flapper: {
      snurk: 'hello',
      bob: 'xxx'
    }
  })

  // await client.set({
  //   $id: 'yeA'
  // })

  await client.set({
    $id: 'yeB',
    parents: ['yeA']
  })

  t.deepEqual(
    await client.get({
      $id: 'yeB',
      flapper: { $inherit: { $merge: true, $type: ['yeshType', 'root'] } }
    }),
    {
      flapper: {
        snurk: 'hello',
        bob: 'xxx'
      }
    }
  )

  const observable = client.observe({
    $id: 'yeB',
    flapper: { $inherit: { $merge: true, $type: ['yeshType', 'root'] } }
  })

  const results = []

  const subs = observable.subscribe(p => {
    results.push(deepCopy(p))
  })

  await wait(1000)

  await client.set({
    $id: 'yeA',
    flapper: {
      snurk: 'snurkels'
    }
  })

  await wait(1000)

  subs.unsubscribe()

  t.deepEqual(results, [
    { flapper: { snurk: 'hello', bob: 'xxx' } },
    { flapper: { snurk: 'snurkels', bob: 'xxx' } }
  ])

  await client.delete('root')
  await client.destroy()
  t.true(true)
})

test.serial('list inherit subscription', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        yesh: { type: 'string' },
        no: { type: 'string' },
        flapper: {
          type: 'object',
          properties: {
            snurk: { type: 'json' },
            bob: { type: 'json' }
          }
        }
      }
    },
    types: {
      yeshType: {
        prefix: 'ye',
        fields: {
          yesh: { type: 'string' },
          flapper: {
            type: 'object',
            properties: {
              snurk: { type: 'json' },
              bob: { type: 'json' }
            }
          }
        }
      }
    }
  })

  await client.set({
    $id: 'root',
    yesh: 'yesh',
    no: 'no'
  })

  await client.set({
    $id: 'yeA',
    yesh: 'yesh a'
  })

  await client.set({
    $id: 'yeB',
    parents: ['yeA']
  })

  for (let i = 0; i < 2; i++) {
    await client.set({
      $id: 'ye' + i,
      parents: ['yeA']
    })
  }

  const observable = client.observe({
    $id: 'yeA',
    flapdrol: {
      id: true,
      yesh: { $inherit: { $type: ['yeshType', 'root'] } },
      $field: 'children',
      $list: true
    }
  })

  const results = []

  const subs = observable.subscribe(p => {
    results.push(deepCopy(p))
  })

  await wait(1000)

  await client.set({
    $id: 'yeA',
    yesh: 'yesh a!'
  })

  await wait(1000)

  await client.set({
    $id: 'yeB',
    yesh: 'yesh b'
  })

  await wait(1000)

  t.deepEqualIgnoreOrder(results, [
    {
      flapdrol: [
        { id: 'ye0', yesh: 'yesh a' },
        { id: 'yeB', yesh: 'yesh a' },
        { id: 'ye1', yesh: 'yesh a' }
      ]
    },
    {
      flapdrol: [
        { id: 'ye0', yesh: 'yesh a!' },
        { id: 'yeB', yesh: 'yesh a!' },
        { id: 'ye1', yesh: 'yesh a!' }
      ]
    },
    {
      flapdrol: [
        { id: 'ye0', yesh: 'yesh a!' },
        { id: 'yeB', yesh: 'yesh b' },
        { id: 'ye1', yesh: 'yesh a!' }
      ]
    }
  ])
  subs.unsubscribe()

  await client.delete('root')
  await client.destroy()
  t.true(true)
})

test.serial('list inherit + field subscription', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        yesh: { type: 'string' },
        no: { type: 'string' },
        flapper: {
          type: 'object',
          properties: {
            snurk: { type: 'json' },
            bob: { type: 'json' }
          }
        }
      }
    },
    types: {
      yeshType: {
        prefix: 'ye',
        fields: {
          no: { type: 'string' },
          yesh: { type: 'string' },
          flapper: {
            type: 'object',
            properties: {
              snurk: { type: 'json' },
              bob: { type: 'json' }
            }
          }
        }
      }
    }
  })

  await client.set({
    $id: 'root',
    yesh: 'yesh'
  })

  await client.set({
    $id: 'yeA',
    yesh: 'yesh a',
    no: 'no'
  })

  await client.set({
    $id: 'yeB',
    parents: ['yeA']
  })

  for (let i = 0; i < 2; i++) {
    await client.set({
      $id: 'ye' + i,
      parents: ['yeA']
    })
  }

  const observable = client.observe({
    $id: 'yeA',
    flapdrol: {
      id: true,
      yesh: {
        $field: 'no',
        $inherit: { $type: ['yeshType', 'root'] }
      },
      $field: 'children',
      $list: true
    }
  })

  const results = []

  const subs = observable.subscribe(p => {
    results.push(deepCopy(p))
  })

  await wait(1000)

  await client.set({
    $id: 'yeA',
    no: 'no!'
  })

  await wait(1000)

  await client.set({
    $id: 'yeB',
    no: 'o yes?'
  })

  const x = await client.get({
    $id: 'yeB',
    id: true,
    yesh: {
      $field: 'no',
      $inherit: { $type: ['yeshType', 'root'] }
    }
  })

  t.deepEqual(
    x,
    {
      id: 'yeB',
      yesh: 'o yes?'
    },
    'get'
  )

  await wait(1000)

  t.deepEqualIgnoreOrder(results, [
    {
      flapdrol: [
        { id: 'ye0', yesh: 'no' },
        { id: 'yeB', yesh: 'no' },
        { id: 'ye1', yesh: 'no' }
      ]
    },
    {
      flapdrol: [
        { id: 'ye0', yesh: 'no!' },
        { id: 'yeB', yesh: 'no!' },
        { id: 'ye1', yesh: 'no!' }
      ]
    },
    {
      flapdrol: [
        { id: 'yeB', yesh: 'o yes?' },
        { id: 'ye0', yesh: 'no!' },
        { id: 'ye1', yesh: 'no!' }
      ]
    }
  ])
  subs.unsubscribe()

  await client.delete('root')
  await client.destroy()
})
