import modify from '../lua/src/modify/index'
let a = modify([
  {
    kind: 'update',
    payload: {
      $id: 'match',
      type: 'match'
    }
  },
  {
    kind: 'update',
    payload: {
      $id: 'test',
      title: { nl: 'test' },
      description: { nl: 'lekker man' }
    }
  },
  {
    kind: 'update',
    payload: {
      $id: 'test_deleted',
      title: { nl: 'test_deleted thing' },
      description: { nl: 'niet lekker man' }
    }
  },
  {
    kind: 'delete',
    payload: 'test_deleted'
  }
])

let b = modify([
  {
    // @ts-ignore
    kind: 'update',
    // @ts-ignore
    payload: {
      $id: 'person',
      type: 'person',
      parents: [a[0]],
      title: { en: 'flurpy man' }
    }
  }
])

let c: any[] = []
for (let i = 0; i < a.length; i++) {
  c[i] = a[i]
}

for (let i = 0; i < b.length; i++) {
  c[i] = b[i]
}

// @ts-ignore
return c
