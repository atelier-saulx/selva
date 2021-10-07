import test from 'ava'
import { connect } from '../../src/index'
import getPort from 'get-port'
import { wait } from '../assertions'
import getQueries from './queries'
import {
  startRegistry,
  startOrigin,
  startSubscriptionManager,
  startSubscriptionRegistry,
} from '@saulx/selva-server'

import '../assertions'

let selvas
let port: number
test.before(async (t) => {
  port = await getPort()

  selvas = [
    await startRegistry({
      port,
    }),
    await startOrigin({
      default: true,
      save: true,
      dir: __dirname,
      registry: { port },
    }),
    await startSubscriptionManager({
      registry: { port },
    }),
    // await startSubscriptionManager({
    //   registry: { port },
    // }),
    // await startSubscriptionManager({
    //   registry: { port },
    // }),
    // await startSubscriptionRegistry({ registry: { port } }),
  ]
})

test.after(async (t) => {
  await Promise.all(selvas.map((selva) => selva.destroy()))
  await t.connectionsAreEmpty()
})

test.serial('tally queries', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  selvas.push(client)
  const queries = getQueries('ed989cdaed')
  await client.get({ $all: true })
  const query = {
    $language: 'de',
    $id: 'ed989cdaed',
    id: true,
    children: {
      startTime: true,
      index: true,
      id: true,
      type: true,
      name: true,
      items: {
        disabled: true,
        type: true,
        id: true,
        name: true,
        index: true,
        parents: true,
        $list: {
          $sort: {
            $field: 'index',
            $order: 'asc',
          },
          $find: {
            $traverse: 'children',
          },
        },
      },
      $list: {
        $sort: {
          $field: 'index',
          $order: 'asc',
        },
        $find: {
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'sequence',
          },
        },
      },
    },
  }
  const label = JSON.stringify(query, null, 2)
  console.time(label)
  await Promise.all(Array.from(Array(1)).map(() => client.get(query)))
  console.timeEnd(label)

  // // await wait(1e3)
  // let index = 0

  // for (const query of queries) {
  //   console.log(index++)
  //   const label = JSON.stringify(query, null, 2)
  //   console.time(label)
  //   await client.get(query)
  //   console.timeEnd(label)
  // }

  // let n = 1
  // const results = []
  // const initClientQueries = async () => {
  //   const client = connect({ port }, { loglevel: 'info' })
  //   selvas.push(client)
  //   const queries = getQueries('ed989cdaed')
  //   await client.get({ $all: true })
  //   await wait(1e3)
  //   for (const query of queries) {
  //     // await wait(100)
  //     // let gt = Date.now()
  //     await client.get(query)
  //     gt = Date.now() - gt
  //     await wait(100)
  //     let ot = Date.now()
  //     await new Promise((resolve) => client.observe(query).subscribe(resolve))
  //     ot = Date.now() - ot
  //     results.push({ obs: ot }) //, get: gt, diff: ot - gt })
  //   }
  // }

  // const promises = []
  // while (n--) {
  //   promises.push(initClientQueries())
  // }

  // await Promise.all(promises)

  // console.log(JSON.stringify(results, null, 2))

  t.pass()
})
