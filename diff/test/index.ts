import test from 'ava'
import diff, { applyPatch } from '@saulx/selva-diff'

test('Array', async t => {
  const a = ['a', 'b', 'c', 'd']
  const b = ['x', 'x', 'a', 'b', 'z', 'c', 'd']
  const patch = diff(a, b)
  // array makes a new one - that is bit of a shame (different then object)
  t.deepEqual(applyPatch(a, patch), b, 'array is equal')
  const largeArr = []
  for (let i = 0; i < 10000; i++) {
    largeArr.push(i)
  }
  const largeArr2 = []
  for (let i = 0; i < 10001; i++) {
    largeArr2.push(i)
  }
  largeArr2.splice(10, 0, 'flap')
  var d = Date.now()
  const largePatch = diff(largeArr, largeArr2)
  console.log('Time to calculate large patch (10k)', Date.now() - d, 'ms')
  t.deepEqual(
    applyPatch(largeArr, largePatch),
    largeArr2,
    'large array is equal after patch'
  )
  const ax = ['x', 'x', 'a', 'b', 'z', 'c', 'd', 'e', 'f', 'g', 'urx']
  const bx = ['a', 'b', 'c', 'd']
  t.deepEqual(
    applyPatch(ax, diff(ax, bx)),
    bx,
    'new array smaller then previous'
  )
})

test('Object', async t => {
  const a = {
    a: 'hello',
    b: 'shurf',
    c: 'snurx',
    d: {
      e: 'x'
    },
    f: [1, 2, 3, 4, 5]
  }
  const b = {
    a: 'BLARF',
    z: true,
    f: [6, 1, 2, 8, 9, 4, 5],
    snurkypants: {
      a: true,
      b: false
    },
    d: {
      e: {
        x: true
      }
    }
  }
  const patch = diff(a, b)

  t.deepEqual(applyPatch(a, patch), b, 'is equal')
})

test('Array + nested object lots the same', async t => {
  const obj = {
    x: true,
    y: true,
    cnt: 324,
    kookiepants: {
      x: true,
      y: {
        g: {
          x: true,
          flurpypants: 'x',
          myText: 'fdwefjwef ewofihewfoihwef weoifh'
        }
      }
    }
  }

  const a = {
    f: []
  }

  const b = {
    f: []
  }

  for (let i = 0; i < 20; i++) {
    a.f.push(JSON.parse(JSON.stringify(obj)))
    b.f.push(JSON.parse(JSON.stringify(obj)))
  }

  b.f[5] = { gurken: true }

  const patch = diff(a, b)

  //   console.dir(patch, { depth: 10 })

  t.deepEqual(applyPatch(a, patch), b, 'is equal')

  b.f.splice(8, 1, { gurky: true })
  b.f.splice(1, 1, { flurb: true })
  b.f.splice(3, 1, { flura: true })
  b.f.splice(10, 1, {
    kookiepants: {
      x: false,
      y: {
        g: {
          myText: 'yuzi pants'
        }
      }
    }
  })

  var d = Date.now()
  const patch2 = diff(a, b)

  console.dir(patch2, { depth: 10 })

  console.log('Make large object patch', Date.now() - d, 'ms')

  var d = Date.now()
  const x = applyPatch(a, patch2)

  // console.dir(x, { depth: 10 })

  // console.log('Apply large object patch', Date.now() - d, 'ms')

  t.deepEqual(x, b, 'insert object')

  // t.pass()
})

test('Array + nested object', async t => {
  const a = {
    a: 'hello',
    f: [
      {
        x: true,
        bla: {
          flap: true
        }
      },
      {
        x: true,
        bla: {
          flap: true
        }
      },
      {
        y: true,
        flurp: {
          flurp: 'x'
        }
      },
      {
        z: true,
        j: true
      }
    ]
  }
  const b = {
    f: [
      {
        x: true,
        bla: {
          flap: true
        }
      },
      {
        // this will yield some strange results for sure...
        x: true,
        bla: {
          flap: true
        }
      },
      {
        y: true,
        flurp: {
          flurp: {
            flurpypants: [1, 2, 3]
          }
        }
      },
      {
        z: true,
        j: true
      },
      {
        id: 10
      },
      {
        id: 20
      }
    ]
  }
  const patch = diff(a, b)

  t.deepEqual(applyPatch(a, patch), b, 'is equal')
})

test('Deep in array', async t => {
  const obj = {
    x: true,
    y: true,
    cnt: 324,
    kookiepants: {
      x: true,
      y: {
        g: {
          x: true,
          flurpypants: 'x',
          myText: 'fdwefjwef ewofihewfoihwef weoifh'
        }
      }
    }
  }

  const a = {
    f: [obj]
  }

  const b = {
    f: [
      {
        kookiepants: {
          x: false,
          y: {
            g: {
              myText: 'yuzi pants'
            }
          }
        }
      }
    ]
  }

  const patch = diff(a, b)

  console.dir(patch, { depth: 10 })

  t.deepEqual(applyPatch(a, patch), b, 'is equal')
})
