import test from 'ava'
import { funA, funB } from "../src/index"

test('one plus two equals three', t => {
  t.is(1 + 2, 3)
})

test('the test module', t => {
  let x = funA({ a: true, b: 1 })
  t.true(x.a)
  t.is(x.b, 2)

  let y = funB({ a: true, c: "hello" })
  t.false(y.a)
  t.is(y.c, "hellohello")
})