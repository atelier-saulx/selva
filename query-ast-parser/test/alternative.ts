import test from 'ava'

const filter = {
  id: { not: 'enB1Gp6Z' },
  type: 'enviroment',
}

const filter2 = [
  [
    [
      {
        id: { not: 'enB1Gp6Z' },
        type: 'enviroment',
      },
      {
        id: { not: 'enB1Gp6Z' },
        x: 20,
      },
    ],
    [{ x: true }, { y: true, z: true }],
  ],
  {
    id: 'root',
    flap: 'flap',
    z: false,
  },
  {
    flap: 'snurk',
    startTime: {
      '>': 100,
      '<': 200,
    },
  },
]

const f2 = {
  $operator: '=',
  $value: 'x',
  $field: 'type',
}

const f3 = [
  {
    $operator: '>',
    $value: 10,
    $field: 'x',
  },
  {
    $operator: '<',
    $value: 20,
    $field: 'y',
  },
]

const f4 = []

const f5 = {
  $value: 1,
  $operator: '=',
  $field: 'x',
  $and: {
    $value: 1,
    $operator: '=',
    $field: 'y',
  },
}

const checkIfOld = (f) => {
  if (Array.isArray(f)) {
    if (f.length === 0) {
      return true
    }
    for (const x of f) {
      if (checkIfOld(x)) {
        return true
      }
    }
  }
  if (typeof f === 'object') {
    if (f.$operator || f.$and || f.$or) {
      return true
    }
  }
}

const convertFilter = (filter, nested?) => {
  if (!nested && checkIfOld(filter)) {
    return filter
  }
  const andFilters = []
  if (Array.isArray(filter)) {
    // add nested ors
    let first
    let last
    for (const x of filter) {
      const p = convertFilter(x, true)
      const parsed = p[0]
      if (p.length > 1) {
        let x = parsed

        // ----------------------
        // this is not possible in the lang
        while (x.$and) {
          x = x.$and
        }
        // ----------------------
        for (let i = 1; i < p.length; i++) {
          x.$and = p[i]
          x = p[i]
        }
      }
      if (last) {
        while (last.$or) {
          last = last.$or
        }
        last.$or = parsed
      }
      if (!first) {
        first = parsed
      }
      last = parsed
    }
    return [first]
  } else {
    for (const k in filter) {
      let x = filter[k]
      if (typeof x !== 'object' && !Array.isArray(x)) {
        x = { '=': x }
      }
      for (const o in x) {
        andFilters.push({
          $field: k,
          $operator: o,
          $value: x[o],
        })
      }
    }
  }
  return andFilters
}

test.serial('hello', async (t) => {
  console.info(JSON.stringify(convertFilter(filter), null, 2))

  console.info('---------------------------')
  console.info(JSON.stringify(convertFilter(filter2), null, 2))

  console.info('---------------------------')
  console.info(JSON.stringify(convertFilter(f2), null, 2))

  console.info('---------------------------')
  console.info(JSON.stringify(convertFilter(f3), null, 2))

  console.info('---------------------------')
  console.info(JSON.stringify(convertFilter(f4), null, 2))

  console.info('---------------------------')
  console.info(JSON.stringify(convertFilter(f5), null, 2))

  t.pass()
})
