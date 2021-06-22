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
  srv = await start({
    port,
  })
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          name: { type: 'string', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          date: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
        },
      },
      thing: {
        prefix: 'th',
        fields: {
          title: { type: 'text' },
          ary: {
            type: 'array',
            items: {
              type: 'object',
              properties: {
                title: { type: 'text' },
                name: { type: 'string', search: { type: ['TAG'] } },
                value: {
                  type: 'number',
                  search: { type: ['NUMERIC', 'SORTABLE'] },
                },
                status: {
                  type: 'number',
                  search: { type: ['NUMERIC', 'SORTABLE'] },
                },
                date: {
                  type: 'number',
                  search: { type: ['NUMERIC', 'SORTABLE'] },
                },
                intAry: {
                  type: 'array',
                  items: { type: 'int' },
                },
              },
            },
          },
          intAry: {
            type: 'array',
            items: { type: 'int' },
          },
        },
      },
    },
  })
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('subscription array', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const thing = await client.set({
    type: 'thing',
    title: { en: 'thing' },
  })

  const matches = []

  await wait(500)

  for (let i = 0; i < 10; i++) {
    matches.push({
      $id: thing,
      ary: {
        $push: {
          name: 'match ' + i,
          value: i,
          status: i < 5 ? 100 : 300,
        },
      },
    })
  }

  await Promise.all(matches.map((v) => client.set(v)))

  await wait(500)
  console.log(
    await client.get({
      $id: thing,
      ary: {
        name: true,
        $list: true,
      },
    })
  )

  const obs = client.observe({
    $id: thing,
    ary: true,
  })
  let cnt = 0
  const sub = obs.subscribe((d) => {
    console.log('sub 1', d)
    cnt++
  })

  await wait(1000)
  t.is(cnt, 1)

  await client.set({
    $id: thing,
    ary: {
      $assign: {
        $idx: 0,
        $value: {
          name: 'FLURP!',
        },
      },
    },
  })

  await wait(1000)
  t.is(cnt, 2)
  sub.unsubscribe()

  const obs2 = client.observe({
    $id: thing,
    $language: 'en',
    ary: {
      name: true,
      title: true,
      type: true,
      $list: true,
    },
  })

  let cnt2 = 0
  const sub2 = obs2.subscribe((d) => {
    console.log('sub 2', d)
    cnt2++
  })

  await wait(2000)

  await client.set({
    $id: thing,
    ary: {
      $assign: {
        $idx: 0,
        $value: {
          title: { en: 'Flapdrol' },
        },
      },
    },
  })

  await wait(2000)
  sub2.unsubscribe()
  t.is(cnt2, 2)
  await client.destroy()
})

test.serial('subscription num array', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const thing = await client.set({
    type: 'thing',
    title: { en: 'thing' },
  })

  const matches = []

  await wait(500)

  for (let i = 0; i < 10; i++) {
    matches.push({
      $id: thing,
      intAry: {
        $push: i,
      },
    })
  }

  await Promise.all(matches.map((v) => client.set(v)))

  await wait(500)
  const obs = client.observe({
    $id: thing,
    intAry: true,
  })
  let cnt = 0
  const sub = obs.subscribe((d) => {
    console.log('sub 1', d)
    cnt++
  })

  await wait(1000)

  await client.set({
    $id: thing,
    intAry: {
      $assign: {
        $idx: 0,
        $value: 982,
      },
    },
  })

  await wait(1000)
  t.is(cnt, 2)

  await client.destroy()
})

test.serial('subscription array in object array', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const thing = await client.set({
    type: 'thing',
    title: { en: 'thing' },
  })

  const matches = []

  await wait(500)

  for (let i = 0; i < 10; i++) {
    matches.push({
      $id: thing,
      ary: {
        $push: {
          name: 'match ' + i,
          intAry: {
            $push: i,
          },
        },
      },
    })
  }

  await Promise.all(matches.map((v) => client.set(v)))

  await wait(500)
  const obs = client.observe({
    $id: thing,
    ary: true,
  })
  let cnt = 0
  const sub = obs.subscribe((d) => {
    console.log('sub 1', JSON.stringify(d, null, 2))
    cnt++
  })

  await wait(1000)

  await client.set({
    $id: thing,
    ary: {
      $insert: {
        $idx: 0,
        $value: {
          name: 'match 99',
        },
      },
    },
  })

  await wait(1000)

  await client.set({
    $id: thing,
    ary: {
      $assign: {
        $idx: 2,
        $value: {
          intAry: {
            $push: 99,
          },
        },
      },
    },
  })

  await wait(1000)

  t.is(cnt, 3)

  await client.destroy()
})
