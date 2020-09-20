import test from 'ava'
import { diff, apply } from '@saulx/selva-diff'

test('Diff object (basic)', async t => {
  console.log('go')

  const a = {
    a: 'hello',
    b: 'shurf',
    c: 'snurx',
    d: {
      e: 'x'
    },
    x: [1, 2]
  }

  const b = {
    a: 'BLARF',
    z: true,
    x: [1, 2, 3],
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
