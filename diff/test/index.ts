import test from 'ava'
import { diff, apply } from '@saulx/selva-diff'

import stringA from './exampleStringA'
import stringB from './exampleStringB'

test('Diff object (basic)', async t => {
  const a = {
    a: 'hello',
    b: 'shurf',
    c: 'snurx',
    d: {
      e: 'x'
    },
    x: [{ a: true }, 1, 2, 3, 4, { c: true }]
  }

  // in array we want INSERT and move
  const b = {
    a: 'BLARF',
    z: true,
    x: [{ b: true }, 1, 2, { a: true }, 'x', 'x', 3, 'x', 4, { c: true }],
    snurkypants: {
      a: true,
      b: false
    }
  }

  const firstPatch = diff(a, b)

  const r = apply(a, firstPatch)

  // console.log('frist', JSON.stringify(firstPatch, null, 2))
  t.pass()
})
