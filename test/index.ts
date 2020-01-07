import test from 'ava'
import { funA, funB, connect } from '../src/index'

test('one plus two equals three', t => {
  t.is(1 + 2, 3)
})

test('the test module', t => {
  let x = funA({ a: true, b: 1 })
  t.true(x.a)
  t.is(x.b, 2)

  let y = funB({ a: true, c: 'hello' })
  t.false(y.a)
  t.is(y.c, 'hellohello')
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

  // new types what this means is that the client allways needs to load a map
})
