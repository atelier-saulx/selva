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
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    types: {
      cyclic: {
        prefix: 'cy',
        fields: {
          value: { type: 'number' },
          next: { type: 'reference' },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await wait(100)

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('children cycle: delete root', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'cy1',
    value: 1,
    children: [
      {
        $id: 'cy2',
        value: 2,
        children: [
          {
            $id: 'cy3',
            value: 3,
          }
        ]
      },
    ],
  })
  await client.set({
    $id: 'cy1',
    parents: [ 'root', 'cy3' ]
  })

  // Note that $recursive is required to delete cycles that are descendants of the
  // deleted node.
  t.deepEqualIgnoreOrder(await client.delete({ $id: 'root', $returnIds: true, $recursive: true }), [
    'root', 'cy1', 'cy2', 'cy3',
  ])
  t.deepEqual(await client.get({ $id: 'root', descendants: true }), {
    descendants: []
  })
  t.deepEqual(await client.get({ $id: 'cy1', id: true }), { $isNull: true })
  t.deepEqual(await client.get({ $id: 'cy2', id: true }), { $isNull: true })
  t.deepEqual(await client.get({ $id: 'cy3', id: true }), { $isNull: true })

  await client.destroy()
})

test.serial('children cycle: delete first node', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'cy1',
    value: 1,
    children: [
      {
        $id: 'cy2',
        value: 2,
        children: [
          {
            $id: 'cy3',
            value: 3,
          }
        ]
      },
    ],
  })
  await client.set({
    $id: 'cy3',
    children: [ 'cy1' ]
  })

  t.deepEqualIgnoreOrder(await client.delete({ $id: 'cy1', $returnIds: true }), [
    'cy1', 'cy2', 'cy3',
  ])
  t.deepEqual(await client.get({ $id: 'root', descendants: true }), {
    descendants: []
  })
  t.deepEqual(await client.get({ $id: 'cy1', id: true }), { $isNull: true })
  t.deepEqual(await client.get({ $id: 'cy2', id: true }), { $isNull: true })
  t.deepEqual(await client.get({ $id: 'cy3', id: true }), { $isNull: true })

  await client.destroy()
})

test.serial('delete ref', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'cy1',
    value: 1,
    next: {
      $id: 'cy2',
      value: 2,
      next: {
        $id: 'cy3',
      },
    },
  })
  await client.set({
    $id: 'cy3',
    parents: [],
    next: 'cy1',
  })

  t.deepEqual(
    await client.get({
      $id: 'cy1',
      next: true,
    }),
    { next: 'cy2' }
  )
  t.deepEqual(
    await client.get({
      $id: 'cy2',
      next: true,
    }),
    { next: 'cy3' }
  )
  t.deepEqual(
    await client.get({
      $id: 'cy3',
      next: true,
    }),
    { next: 'cy1' }
  )

  t.deepEqualIgnoreOrder(await client.delete({ $id: 'cy1', $returnIds: true }), [
    'cy1'
  ])
  t.deepEqual(await client.get({ $id: 'cy1', id: true }), { $isNull: true })
  t.deepEqual(await client.get({ $id: 'cy2', id: true }), { id: 'cy2' })
  t.deepEqual(await client.get({ $id: 'cy3', id: true }), { id: 'cy3' })

  await client.destroy()
})
