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
      $id: 'viA',
      title: {
        en: 'nice!'
      },
      value: 25,
      auth: {
        // role needs to be different , different roles per scope should be possible
        role: {
          id: ['root'],
          type: 'admin'
        }
      }
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
      title: { en: 'flurpy man', de: 'ach so' }
    }
  },
  {
    // @ts-ignore
    kind: 'delete',
    // @ts-ignore
    payload: 'root'
  }
])

let d = modify([
  {
    // @ts-ignore
    kind: 'update',
    // @ts-ignore
    payload: {
      $id: 'match',
      children: []
    }
  },
  {
    // @ts-ignore
    kind: 'update',
    // @ts-ignore
    payload: {
      $id: 'person',
      $merge: false,
      title: { de: 'fchlurpy mann' }
    }
  },
  {
    // @ts-ignore
    kind: 'delete',
    // @ts-ignore
    payload: 'root'
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
