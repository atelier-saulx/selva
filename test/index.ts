import test from 'ava'
// expose types here (default), maybe expose things like id etc
import { connect } from '../src/index'
import { start } from 'selva-server'

let db

const wait = () => new Promise(r => setTimeout(r, 500))

test.before(async t => {
  db = await start({ port: 6061, modules: ['redisearch'] })
  await wait() // for now until client is ready
  // This runs before all tests
})

test('generates a unique id', async t => {
  const client = connect({
    port: 6060,
    host: '127.0.1.1'
  })
  const id1 = client.id({ type: 'match' })
  const id2 = client.id({ type: 'match' })

  t.true(id1 !== id2)
  t.true(/ma.+/.test(id1))
  // new types what this means is that the client allways needs to load a map add it to prefix
  // allways subscribe on it (hash)
})

test('set', async t => {
  const client = connect({
    port: 6060,
    host: '127.0.1.1'
  })

  await client.set({
    type: 'root',
    $id: 'root'
  })

  await client.set({
    type: 'match',
    title: { en: 'hello' }
  })
})
