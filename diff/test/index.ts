import test from 'ava'
import { diff, apply } from '@saulx/selva-diff'

import { calcPatch } from '@saulx/selva-diff/lcs'

// implement LCS for arrays

test('Diff object (basic)', async t => {
  console.log('go')

  const a1 = [1, 2, 3, 4, 5]

  const b2 = [323, 2, 123, 123, 1221, 11]

  const x = calcPatch(a1, b2)

  console.log(x)

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

  console.log('frist', JSON.stringify(firstPatch, null, 2))
  t.pass()
})
