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
      match: {
        prefix: 'ma',
        fields: {
          title: {
            type: 'text',
          },
          published: { type: 'boolean' },
          awayTeam: { type: 'reference' },
          homeTeam: { type: 'reference' },
          image: {
            type: 'object',
            properties: {
              thumb: {
                type: 'url',
              },
              cover: {
                type: 'url',
              },
            },
          },
          video: {
            type: 'record',
            values: {
              type: 'object',
              properties: {
                hls: {
                  type: 'url',
                },
                mp4: {
                  type: 'url',
                },
              },
            },
          },
          authors: {
            type: 'set',
            items: { type: 'string' },
          },
          floats: {
            type: 'set',
            items: { type: 'float' },
          },
          integers: {
            type: 'set',
            items: { type: 'int' },
          },
          updatedAt: {
            type: 'timestamp',
          },
        },
      },
      team: {
        prefix: 'te',
        fields: {
          updatedAt: {
            type: 'timestamp',
          },
        },
      },
      competition: {
        prefix: 'co',
      },
      camera: {
        prefix: 'ca',
      },
    },
  })
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.info('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('updatedAt only changes when actually changed', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const team1 = await client.setWithMeta({
    type: 'team',
    $alias: 'myteam1',
    name: 'team1',
  })

  t.deepEqual(team1.updated, true, 'team1 should have updated on first set')

  const team2 = await client.setWithMeta({
    type: 'team',
    $alias: 'myteam2',
    name: 'team2',
  })

  t.deepEqual(team2.updated, true, 'team2 should have updated on first set')

  const competition = await client.setWithMeta({
    type: 'competition',
    $alias: 'mycompetition',
    name: 'competition',
  })

  t.deepEqual(
    competition.updated,
    true,
    'competition should have updated on first set'
  )

  const cam1 = await client.setWithMeta({
    type: 'camera',
  })

  t.deepEqual(cam1.updated, true, 'cam1 should have updated on first set')

  let n = 2
  let lastUpdatedAt

  while (n--) {
    const meta = await client.setWithMeta({
      type: 'match',
      $alias: 'snurkle',
      $language: 'en',
      published: true,
      title: 'success',
      homeTeam: team1.id,
      awayTeam: team2.id,
      image: {
        thumb: 'https://example.com',
        cover: 'https://example.com',
      },
      video: {
        default: {
          hls: 'https://example.com',
          mp4: 'https://example.com',
        },
      },
      authors: ['leif', 'rolf'],
      floats: [1.5, 2.5],
      integers: [1, 2],
      parents: {
        $add: [team1.id, team2.id, competition.id],
      },
      children: {
        $value: [cam1.id],
      },
    })

    if (n === 1) {
      t.deepEqual(meta.updated, true, 'first set should update')
    } else {
      t.deepEqual(meta.updated, false, 'second set should not update')
    }

    const { updatedAt } = await client.get({
      $alias: 'snurkle',
      updatedAt: true,
    })

    if (lastUpdatedAt) {
      t.is(lastUpdatedAt, updatedAt)
    }

    lastUpdatedAt = updatedAt

    await wait(1)
  }

  await client.delete('root')
  await client.destroy()
})

test.serial('Subscribe to updatedAt', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const team1 = await client.set({
    type: 'team',
    name: 'team1',
  })

  t.plan(1)
  const obs = client.observe({
    $id: team1,
    updatedAt: true,
  })
  let ts = 0
  const sub = obs.subscribe((v) => {
    if (ts === 0) {
      ts = v.updatedAt
    } else {
      t.assert(v.updatedAt > ts, 'updatedAt should have changed')
    }
  })
  await wait(1e2)

  await client.set({
    $id: team1,
    name: 'lolteam',
  })
  await wait(1e2)
  sub.unsubscribe()
  await wait(1e2)

  await client.delete('root')
  await client.destroy()
})
