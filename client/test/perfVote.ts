import { performance } from 'perf_hooks';
import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'
import {SetOptions} from '../src/set'
const { RateLimit } = require('async-sema')

let srv
let port: number

test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
})

test.beforeEach(async t => {
  const client = connect({ port })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      show: {
        prefix: 'sh',
        fields: {
          title: { type: 'text' },
          votes: { type: 'number' }
        }
      },
      vote: {
        prefix: 'vo',
        fields: {
          uid: { type: 'string' }
        }
      }
    }
  })
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  //await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('voting w/parsers', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  const sh = await Promise.all([
    client.set({
      $language: 'en',
      type: 'show',
      title: 'LOL',
      votes: 0,
    }),
    client.set({
      $language: 'en',
      type: 'show',
      title: 'ROFL',
      votes: 0,
    })
  ])

  const nrVotes = 30000
  const votesPerSecond = 6200
  const votes: SetOptions[] = Array.from(Array(nrVotes).keys()).map(v => ({
      $id: sh[v & 1],
      votes: { $increment: 1 },
      children: {
        $add: [ { type: 'vote', uid: `user${v}` } ]
      }
  }))

  const lim = RateLimit(votesPerSecond, { timeUnit: 1000 });

  const start = performance.now()
  await Promise.all(votes.map(async (vote) => {
    await lim()
    await client.set(vote)
  }))
  const end = performance.now()
  const tTotal = end - start

  console.log('tTotal:', tTotal / 1000)
  t.true(tTotal / 1000 < 30)

  // await new Promise(r => setTimeout(r, 1e8))
})
