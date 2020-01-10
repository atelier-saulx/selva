import test from 'ava'
import { connect } from '../src/index'

test('generates a unique id from type', async t => {
  const client = connect({
    port: 6061
  })
  const id1 = client.id({ type: 'match' })
  const id2 = client.id({ type: 'match' })
  t.true(id1 !== id2)
  t.true(/ma.+/.test(id1))
  // new types what this means is that the client allways needs to load a map add it to prefix
  // allways subscribe on it (hash)
})
