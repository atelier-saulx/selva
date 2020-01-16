import modify from '../lua/src/modify/index'
let a = modify([
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
// @ts-ignore
return a
