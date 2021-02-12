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
      sport: {
        prefix: 'sp',
        fields: {
          title: { type: 'text' },
          menu: { type: 'references' },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          menu: { type: 'references' },
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

test.serial('inherit references $list', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  const menuItem = await client.set({
    $language: 'en',
    type: 'match',
    title: 'menu item',
  })

  const sport = await client.set({
    $language: 'en',
    type: 'sport',
    title: 'football',
    menu: [menuItem],
  })

  const child = await client.set({
    $language: 'en',
    type: 'match',
    title: 'football match',
    parents: [sport],
  })

  t.deepEqual(
    await client.get({
      $id: child,
      $language: 'en',
      menu: {
        title: true,
        $list: {
          $inherit: { $type: ['sport'] },
        },
      },
    }),
    {
      menu: [
        {
          title: 'menu item',
        },
      ],
    }
  )
  await client.destroy()
})
