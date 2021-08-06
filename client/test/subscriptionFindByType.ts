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
      league: {
        prefix: 'le',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          thing: { type: 'string', search: { type: ['EXISTS'] } },
          matches: { type: 'references' },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          description: { type: 'text' },
          value: {
            type: 'number',
            search: { type: ['NUMERIC', 'SORTABLE', 'EXISTS'] },
          },
          status: { type: 'number', search: { type: ['NUMERIC'] } },
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

test.serial('subscription find by type', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'le1',
    type: 'league',
    name: 'league 1',
  })

  await client.set({
    $id: 'ma1',
    parents: ['le1'],
    type: 'match',
    name: 'match 1',
    value: 1,
  })

  await client.set({
    $id: 'ma2',
    parents: ['ma1'],
    type: 'match',
    name: 'match 2',
    value: 2,
  })

  await client.set({
    $id: 'ma3',
    parents: ['ma1'],
    type: 'match',
    name: 'match 3',
  })

  await client.set({
    $id: 'le1',
    matches: ['ma1', 'ma2', 'ma3'],
  })

  t.plan(3)

  let cnt1 = 0
  const sub1 = client
    .observe({
      $id: 'root',
      id: true,
      items: {
        name: true,
        nonsense: { $default: 'yes' },
        $list: {
          $find: {
            $recursive: true,
            $traverse: {
              root: 'children',
              league: 'children',
              $any: false,
            },
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match',
              },
              {
                $field: 'value',
                $operator: 'exists',
              },
            ],
          },
        },
      },
    })
    .subscribe((v) => {
      if (cnt1 === 0) {
        t.deepEqual(v, {
          id: 'root',
          items: [
            { name: 'match 1', nonsense: 'yes' },
            // { name: 'match 2', nonsense: 'yes' },
          ],
        })
      } else {
        t.fail()
      }
      cnt1++
    })

  let cnt2 = 0
  const sub2 = client
    .observe({
      $id: 'root',
      id: true,
      items: {
        name: true,
        nonsense: { $default: 'yes' },
        $list: {
          $find: {
            $recursive: true,
            $traverse: {
              root: 'children',
              league: { $first: ['matches', 'children'] },
              $any: false,
            },
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match',
              },
              {
                $field: 'value',
                $operator: 'exists',
              },
            ],
          },
        },
      },
    })
    .subscribe((v) => {
      if (cnt2 === 0) {
        t.deepEqual(v, {
          id: 'root',
          items: [
            { name: 'match 1', nonsense: 'yes' },
            { name: 'match 2', nonsense: 'yes' },
          ],
        })
      } else if (cnt2 === 1) {
        t.deepEqual(v, {
          id: 'root',
          items: [
            { name: 'match 1', nonsense: 'yes' },
            { name: 'match 2', nonsense: 'yes' },
            { name: 'match 4', nonsense: 'yes' },
          ],
        })
        t.fail()
      }
      cnt2++
    })

  await wait(2e3)

  await Promise.all([
    client.set({
      $id: 'ma4',
      parents: ['ma1'],
      type: 'match',
      name: 'match 4',
      value: 4,
    }),

    client.set({
      $id: 'le1',
      matches: { $add: 'ma4' },
    }),
  ])

  await wait(2e3)

  sub1.unsubscribe()
  sub2.unsubscribe()
  await client.destroy()
})
