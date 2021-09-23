import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import getPort from 'get-port'

let srv
let port: number
test.before(async (t) => {
  port = await getPort()
  srv = await start({
    dir: '/Users/youzi/Downloads',
    port,
  })
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  // await t.connectionsAreEmpty()
})

test.serial('Do it', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  const all = await client.get({
    $id: 'sh3e6ee217',
    $all: true,
    parents: true,
    children: true,
    descendants: {
      $all: true,
      $list: true,
    },
  })

  const { descendants = [] } = await client.get({
    $id: 'sh3e6ee217',
    descendants: {
      id: true,
      parents: true,
      children: true,
      $list: true,
    },
  })

  for (const item of descendants) {
    const index = all.descendants.findIndex(({ id }) => id === item.id)
    const target = all.descendants[index]
    if (target.type === 'pageTemplate') {
      all.descendants.splice(index, 1)
    } else {
      Object.assign(target, item)
    }
  }

  require('fs').writeFileSync(
    '/Users/youzi/Downloads/dump.json',
    JSON.stringify(all, null, 2)
  )
  // await client.updateSchema({
  //   types: {
  //     sport: {
  //       prefix: 'sp',
  //     },
  //   },
  // })

  // await client.set({
  //   type: 'sport',
  //   $id: 'sp11',
  //   parents: {
  //     $add: [],
  //   },
  // })

  // t.deepEqualIgnoreOrder(await client.get({ $id: 'sp11', ancestors: true }), {
  //   ancestors: ['root'],
  // })

  await client.destroy()
})
