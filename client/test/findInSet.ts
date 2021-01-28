import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  const client = connect({ port })
  await client.updateSchema({
    types: {
      user: {
        fields: {
          roles: {
            type: 'set',
            search: {
              type: ['TAG']
            },
            items: {
              type: 'string'
            }
          },
          numbers: {
            type: 'set',
            items: {
              type: 'number'
            }
          },
          sinks: {
            type: 'set',
            items: {
              type: 'float'
            }
          },
          ints: {
            type: 'set',
            items: {
              type: 'int'
            }
          }
        }
      }
    }
  })
})

test.after(async t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('search user roles', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    type: 'user',
    roles: ['club', 'club:id1']
  })

  const id = await client.set({
    type: 'user',
    roles: ['club', 'club:id2']
  })

  t.is(
    (
      await client.get({
        descendants: {
          id: true,
          $list: {
            $find: {
              $filter: {
                $field: 'roles',
                $operator: 'has',
                $value: 'club:id1'
              }
            }
          }
        }
      })
    ).descendants.length,
    1
  )

  t.is(
    (
      await client.get({
        descendants: {
          id: true,
          $list: {
            $find: {
              $filter: {
                $field: 'roles',
                $operator: 'has',
                $value: 'club:id2'
              }
            }
          }
        }
      })
    ).descendants.length,
    1
  )

  t.is(
    (
      await client.get({
        descendants: {
          id: true,
          $list: {
            $find: {
              $filter: {
                $field: 'roles',
                $operator: 'has',
                $value: 'club'
              }
            }
          }
        }
      })
    ).descendants.length,
    2
  )

  t.is(
    (
      await client.get({
        descendants: {
          id: true,
          $list: {
            $find: {
              $filter: {
                $field: 'roles',
                $operator: 'has',
                $value: 'rando'
              }
            }
          }
        }
      })
    ).descendants.length,
    0
  )

  await client.destroy()
})

test.serial('search user numbers', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  const id = await client.set({
    type: 'user',
    numbers: [1, 2.4, 3, 4],
    sinks: [1, 7.0, 4.5, 8.25],
    ints: [57082, 0x01234567, 16435934]
  })

  t.is(
    (
      await client.get({
        descendants: {
          id: true,
          $list: {
            $find: {
              $filter: {
                $field: 'numbers',
                $operator: 'has',
                $value: 1
              }
            }
          }
        }
      })
    ).descendants.length,
    1
  )

  t.is(
    (
      await client.get({
        descendants: {
          id: true,
          $list: {
            $find: {
              $filter: {
                $field: 'sinks',
                $operator: 'has',
                $value: 7.0
              }
            }
          }
        }
      })
    ).descendants.length,
    1
  )

  t.is(
    (
      await client.get({
        descendants: {
          id: true,
          $list: {
            $find: {
              $filter: {
                $field: 'ints',
                $operator: 'has',
                $value: 16435934
              }
            }
          }
        }
      })
    ).descendants.length,
    1
  )

  await client.destroy()
})
