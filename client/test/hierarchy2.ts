import test from 'ava'
import { connect } from '../src/index'
import { start, s3Backups } from '@saulx/selva-server'
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
    languages: ['en', 'de'],
    types: {
      sport: {
        prefix: 'sp',
      },
      club: {
        prefix: 'cl',
      },
      competition: {
        prefix: 'co',
      },
      team: {
        prefix: 'te',
        hierarchy: {
          club: {
            excludeAncestryWith: ['sport'],
          },
        },
      },
      match: {
        prefix: 'ma',
        hierarchy: {
          team: {
            // this messes it up!
            excludeAncestryWith: ['competition'],
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

test.serial.skip('correct hierachy rules', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const football = await client.set({
    type: 'sport',
  })

  const bball = await client.set({
    type: 'sport',
  })

  const eredivisie2020 = await client.set({
    type: 'competition',
    parents: [football],
  })

  const ajax = await client.set({
    type: 'club',
    parents: [football, bball],
  })

  const ajax1 = await client.set({
    type: 'team',
    parents: [ajax, eredivisie2020],
  })

  const ajax2 = await client.set({
    type: 'team',
    parents: [ajax, eredivisie2020],
  })

  const match = await client.set({
    type: 'match',
    parents: [ajax1, ajax2, eredivisie2020],
  })

  t.deepEqualIgnoreOrder(await client.get({ $id: match, ancestors: true }), {
    ancestors: ['root', football, eredivisie2020, ajax1, ajax2, ajax],
  })

  await client.delete('root')
  await client.destroy()
})
