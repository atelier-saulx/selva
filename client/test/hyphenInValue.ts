import test from 'ava'
import { connect } from '@saulx/selva'
import { start } from '@saulx/selva-server'
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
    // languages: ['en'],
    types: {
      actor: {
        prefix: 'ac',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          born: { type: 'int' },
          died: { type: 'int' }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('get value without hyphen', async t => {
  const client = connect({ port })

  await Promise.all(
    [
      {
        name: 'Charlton Heston',
        born: 1923,
        died: 2008
      },
      {
        name: 'Leigh TaylorYoung',
        born: 1945
      }
    ].map(actor =>
      client.set({
        type: 'actor',
        ...actor
      })
    )
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      items: {
        name: true,
        $list: {
          $find: {
            $traverse: 'children',
            $filter: [
              { $field: 'type', $operator: '=', $value: 'actor' },
              { $field: 'name', $operator: '=', $value: 'Leigh TaylorYoung' }
            ]
          }
        }
      }
    }),
    {
      items: [{ name: 'Leigh TaylorYoung' }]
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('get value with hyphen', async t => {
  const client = connect({ port })

  await Promise.all(
    [
      {
        name: 'Charlton Heston',
        born: 1923,
        died: 2008
      },
      {
        name: 'Leigh Taylor-Young',
        born: 1945
      }
    ].map(actor =>
      client.set({
        type: 'actor',
        ...actor
      })
    )
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      items: {
        name: true,
        $list: {
          $find: {
            $traverse: 'children',
            $filter: [
              { $field: 'type', $operator: '=', $value: 'actor' },
              { $field: 'name', $operator: '=', $value: 'Leigh Taylor-Young' }
            ]
          }
        }
      }
    }),
    {
      items: [{ name: 'Leigh Taylor-Young' }]
    }
  )

  await client.delete('root')
  await client.destroy()
})
