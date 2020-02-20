import * as redis from '../lua/src/redis'

import modify from '../lua/src/modify/index'
redis.debug('ID TESTING: ' + redis.id())
let a = modify([
  {
    // @ts-ignore
    kind: 'update',
    // @ts-ignore
    payload: {
      type: 'match',
      video: {
        mp4: 'https://flappie.com/clowns.mp4'
      },
      flurpbird: 'hello',
      date: 100000,
      title: {
        en: 'best match'
      },
      children: [
        {
          type: 'person',
          parents: { $add: 'root' }
        }
      ],
      flapperdrol: { smurky: true }
    }
  },
  {
    // @ts-ignore
    kind: 'update',
    payload: {
      $id: 'match',
      type: 'match'
    }
  },
  {
    // @ts-ignore
    kind: 'update',
    // @ts-ignore
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
    // @ts-ignore
    kind: 'update',
    payload: {
      $id: 'test_deleted',
      title: { nl: 'test_deleted thing' },
      description: { nl: 'niet lekker man' }
    }
  },
  {
    // @ts-ignore
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
      children: [
        {
          type: 'match',
          title: {
            nl: 'child1'
          }
        },
        {
          type: 'match',
          title: {
            nl: 'child2'
          }
        },
        {
          type: 'match',
          title: {
            nl: 'child3'
          }
        }
      ]
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
