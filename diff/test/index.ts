import test from 'ava'
import diff, { applyPatch } from '@saulx/selva-diff'
import region from './examples/region.json'

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
  largeArr2.splice(5000, 0, 'flap')
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

  for (let i = 0; i < 1000; i++) {
    a.f.push(JSON.parse(JSON.stringify(obj)))
    b.f.push(JSON.parse(JSON.stringify(obj)))
  }

  b.f[5] = { gurken: true }

  const patch = diff(a, b)

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

  // can add optimization techniques to not send the diff is the
  // diff is larger then the new object (on every level)

  const patch = diff(a, b)

  t.deepEqual(applyPatch(a, patch), b, 'is equal')
})

test('Real life only components', async t => {
  const a = JSON.parse(JSON.stringify(region.components.slice(0, 1)))
  const b = JSON.parse(JSON.stringify(a))
  b[0].children.shift()
  b[0].children.shift()
  b[0].children.shift()
  const patch2 = diff(a, b)
  const x = applyPatch(a, patch2)
  t.deepEqual(x, b, 'is equal after games put to live')
})

test('Real life', async t => {
  const a = JSON.parse(JSON.stringify(region))
  const b = JSON.parse(JSON.stringify(a))
  // can add optimization techniques to not send the diff is the
  // diff is larger then the new object (on every level)

  const patch = diff(a, b)
  t.is(patch, undefined, 'is the same no diff')
  t.deepEqual(applyPatch(a, patch), b, 'is equal')

  b.components[1].children = [
    JSON.parse(JSON.stringify(b.components[3].children[0])),
    JSON.parse(JSON.stringify(b.components[3].children[1])),
    JSON.parse(JSON.stringify(b.components[3].children[2]))
  ]

  b.components[3].children.shift()
  b.components[3].children.shift()
  b.components[3].children.shift()

  var d = Date.now()
  const patch2 = diff(a, b)
  console.log('Make sstv patch', Date.now() - d, 'ms')

  var d = Date.now()
  const x = applyPatch(a, patch2)
  console.log('Apply sstv patch', Date.now() - d, 'ms')

  t.deepEqual(x, b, 'is equal after games put to live')
})

test('Real life - theme', async t => {
  const a = JSON.parse(JSON.stringify(region))
  const b = JSON.parse(JSON.stringify(a))

  b.theme.colors.backgroundPastel = 'rgb(255,0,0)'

  var d = Date.now()
  const patch2 = diff(a, b)
  console.log('Make sstv patch (theme)', Date.now() - d, 'ms')

  var d = Date.now()
  const x = applyPatch(a, patch2)
  console.log('Apply sstv patch (theme)', Date.now() - d, 'ms')

  t.deepEqual(x, b, 'is equal after games put to live')
})

test('Remove', async t => {
  const comp = [
    { a: 1 },
    { b: 1 },
    { c: 1 },
    { d: 1 },
    { e: 1 },
    { snurkels: 'blurf' }
  ]

  const a = comp

  const b = JSON.parse(JSON.stringify(comp))

  b.shift()
  b.shift()
  b.shift()

  const patch = diff(a, b)

  t.deepEqual(applyPatch(a, patch), b, 'is equal')
})

test('Remove nested array', async t => {
  const comp = {
    a: [
      { a: 1 },
      {
        x: 1,
        children: []
      },
      {
        b: 1,
        children: [{ x: true }, { y: true }, { z: true }, { glur: true }]
      },
      { c: 1, children: [{ cx: true }, { cy: true }, { ca: true }] },
      { e: 1, children: [{ cx: true }, { cy: true }, { ca: true }] },
      { snurkels: 'blurf' }
    ]
  }

  const a = comp

  const b = JSON.parse(JSON.stringify(comp))

  b.a[1].children.push({ poop: true })
  b.a[1].children.push({ poop: true })

  b.a[2].children.shift()
  b.a[2].children.shift()

  b.a[3].children.pop()
  b.a[3].children.pop()

  b.a[4].children = []

  const patch = diff(a, b)
  const x = applyPatch(a, patch)

  t.deepEqual(x, b, 'is equal')
})

test('Remove deep', async t => {
  const ax = JSON.parse(JSON.stringify(region))

  const comp = ax.components[3].children

  const a = comp

  const b = JSON.parse(JSON.stringify(comp))

  b.shift()
  b.shift()
  b.shift()

  const patch = diff(a, b)
  const x = applyPatch(a, patch)

  t.deepEqual(x, b, 'is equal')
})

test('Flip', async t => {
  const a = {
    $id: 'root',
    flurp: [1, 2, 3, 4]
  }

  const b = {
    $id: 'root',
    flurp: [1, 3, 2, 4]
  }

  const patch = diff(a, b)
  const x = applyPatch(a, patch)

  t.deepEqual(x, b, 'is equal')
})

test('Weird array', async t => {
  const a = {
    upcoming: [
      { id: 'maug8' },
      { id: 'maug7' },
      { id: 'maug5' },
      { id: 'maug4' },
      { id: 'maug2' },
      { id: 'maug11' },
      { id: 'maug10' },
      { id: 'maug1' },
      { id: 'mau2' },
      { id: 'mau1' }
    ],
    past: [
      { id: 'map8' },
      { id: 'map7' },
      { id: 'map5' },
      { id: 'map4' },
      { id: 'map2' },
      { id: 'map14' },
      { id: 'map13' },
      { id: 'map11' },
      { id: 'map10' },
      { id: 'map1' }
    ],
    live: []
  }

  const b = {
    upcoming: [
      { id: 'mau2' },
      { id: 'maug1' },
      { id: 'maug2' },
      { id: 'maug4' },
      { id: 'maug5' },
      { id: 'maug7' },
      { id: 'maug8' },
      { id: 'maug10' },
      { id: 'maug11' },
      { id: 'maug13' }
    ],
    past: [
      { id: 'map1' },
      { id: 'map2' },
      { id: 'map4' },
      { id: 'map5' },
      { id: 'map7' },
      { id: 'map8' },
      { id: 'map10' },
      { id: 'map11' },
      { id: 'map13' },
      { id: 'map14' }
    ],
    live: [{ id: 'mau1' }]
  }

  const patch = diff(a, b)

  const x = applyPatch(a, patch)

  t.deepEqual(x, b, 'is equal')
})

test('Weird array 2 register copy', async t => {
  const a = {
    upcoming: [
      { id: 'mau1' },
      { id: 'mau2' },
      { id: 'maug1' },
      { id: 'maug2' },
      { id: 'maug4' },
      { id: 'maug5' },
      { id: 'maug7' },
      { id: 'maug8' },
      { id: 'maug10' },
      { id: 'maug11' }
    ],
    past: [
      { id: 'map1' },
      { id: 'map2' },
      { id: 'map4' },
      { id: 'map5' },
      { id: 'map7' },
      { id: 'map8' },
      { id: 'map10' },
      { id: 'map11' },
      { id: 'map13' },
      { id: 'map14' }
    ],
    live: []
  }

  const b = {
    upcoming: [
      { id: 'mau2' },
      { id: 'maug1' },
      { id: 'maug2' },
      { id: 'maug4' },
      { id: 'maug5' },
      { id: 'maug7' },
      { id: 'maug8' },
      { id: 'maug10' },
      { id: 'maug11' }, // re-uses this
      { id: 'maug13' }
    ],
    past: [
      { id: 'map1' },
      { id: 'map2' },
      { id: 'map4' },
      { id: 'map5' },
      { id: 'map7' },
      { id: 'map8' },
      { id: 'map10' },
      { id: 'map11' },
      { id: 'map13' },
      { id: 'map14' }
    ],
    live: [{ id: 'mau1' }]
  }

  const patch = diff(a, b)

  const x = applyPatch(a, patch)

  t.deepEqual(x, b, 'is equal')
})
