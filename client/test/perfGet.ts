import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
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
    languages: ['en', 'de'],
    types: {
      thing: {
        fields: {
          index: { type: 'int' },
        },
      },
    },
  })

  await client.destroy()
})

test.after(async (t) => {
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('do gets', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  const parentsAmount = 100
  const childrenPerParent = 10
  const loops = 10
  // do sets
  const createChildren = (pId = 'root', n = parentsAmount) => {
    const promises = []

    while (n--) {
      promises.push(
        client.set({
          parents: [pId],
          type: 'thing',
          index: n,
        })
      )
    }

    return Promise.all(promises)
  }

  const parents = await createChildren()
  await Promise.all(parents.map((id) => createChildren(id, childrenPerParent)))

  let total = 0
  let n = loops
  while (n--) {
    await wait(50)
    const start = Date.now()
    await client.get({
      children: {
        id: true,
        index: true,
        $list: {
          $sort: {
            $field: 'index',
            $order: 'asc',
          },
        },
        children: {
          id: true,
          index: true,
          $list: {
            $sort: {
              $field: 'index',
              $order: 'asc',
            },
          },
        },
      },
    })
    const end = Date.now()
    total += end - start
  }

  console.log(
    `${parentsAmount} nodes with ${childrenPerParent} children: ${
      total / loops
    }ms avg per loop (${loops} loops)`
  )

  total = 0
  n = loops

  while (n--) {
    await wait(50)

    const start = Date.now()

    const { children, ...res } = await client.get({
      children: {
        id: true,
        index: true,
        $list: {
          $sort: {
            $field: 'index',
            $order: 'asc',
          },
        },
      },
    })

    res.children = await Promise.all(
      children.map(({ id }) => {
        return client.get({
          $id: id,
          children: {
            id: true,
            index: true,
            $list: {
              $sort: {
                $field: 'index',
                $order: 'asc',
              },
            },
          },
        })
      })
    )

    const end = Date.now()
    total += end - start
  }

  console.log('---- now split it up:')
  console.log(
    `${parentsAmount} nodes with ${childrenPerParent} children: ${
      total / loops
    }ms avg per loop (${loops} loops)`
  )

  t.pass()

  await client.destroy()
})
