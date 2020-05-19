import test from 'ava'
import { connect } from '../src/index'
import { start, startOrigin } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv1
let srv2
let port1: number
test.before(async t => {
  port1 = await getPort()
  srv1 = await start({
    port: port1
  })
  const client1 = connect({ port: port1 })
  await client1.updateSchema({
    languages: ['en'],
    types: {
      sport: {
        prefix: 'sp',
        fields: {
          value: { type: 'number', search: true },
          rando: { type: 'string' },
          matches: { type: 'references' },
          match: { type: 'reference' }
        }
      }
    }
  })

  srv2 = await startOrigin({
    name: 'matchdb',
    registry: { port: port1 }
  })

  await client1.updateSchema(
    {
      languages: ['en'],
      types: {
        match: {
          prefix: 'ma',
          fields: {
            rando: { type: 'string' },
            value: { type: 'number', search: true },
            sport: { type: 'reference' },
            sports: { type: 'references' }
          }
        }
      }
    },
    'matchdb'
  )

  await client1.destroy()
})

test.after(async _t => {
  let client = connect({ port: port1 })
  let d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await srv1.destroy()

  d = Date.now()
  await client.delete({ $id: 'root', $db: 'matchdb' })
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv2.destroy()
})

test.serial('$db with nested query', async t => {
  const client1 = connect({ port: port1 }, { loglevel: 'info' })

  await client1.set({
    $id: 'sp1',
    type: 'sport',
    rando: 'rando sport!',
    matches: ['ma1'],
    match: 'ma1'
  })

  await client1.set({
    $db: 'matchdb',
    $id: 'ma1',
    type: 'match',
    rando: 'rando match!'
  })

  t.deepEqualIgnoreOrder(
    await client1.get({
      $id: 'sp1',
      rando: true,
      match: {
        $db: 'matchdb',
        $id: 'ma1',
        rando: true
      }
    }),
    {
      rando: 'rando sport!',
      match: {
        rando: 'rando match!'
      }
    }
  )

  await client1.delete('root')
  await client1.destroy()
})

test.serial.only('$db with reference/references', async t => {
  const client1 = connect({ port: port1 }, { loglevel: 'info' })

  await client1.set({
    $id: 'sp1',
    type: 'sport',
    rando: 'rando sport!',
    matches: ['ma1'],
    match: 'ma1'
  })

  await client1.set({
    $db: 'matchdb',
    $id: 'ma1',
    type: 'match',
    rando: 'rando match!'
  })

  t.deepEqualIgnoreOrder(
    await client1.get({
      $id: 'sp1',
      rando: true,
      match: {
        $db: 'matchdb',
        rando: true
      },
      matches: {
        $db: 'matchdb',
        rando: true,
        $list: true
      }
    }),
    {
      rando: 'rando sport!',
      match: {
        rando: 'rando match!'
      },
      matches: [
        {
          rando: 'rando match!'
        }
      ]
    }
  )

  await client1.delete('root')
  await client1.destroy()
})

test.serial('nested $db with reference/references', async t => {
  const client1 = connect({ port: port1 }, { loglevel: 'info' })

  await client1.set({
    $id: 'sp1',
    type: 'sport',
    rando: 'rando sport!',
    matches: ['ma1'],
    match: 'ma1'
  })

  await client1.set({
    $db: 'matchdb',
    $id: 'ma1',
    type: 'match',
    rando: 'rando match!',
    sport: 'sp1',
    sports: ['sp1']
  })

  t.deepEqualIgnoreOrder(
    await client1.get({
      $id: 'sp1',
      rando: true,
      match: {
        $db: 'matchdb',
        rando: true,
        sport: {
          $db: 'default',
          rando: true
        },
        sports: {
          $db: 'default',
          rando: true,
          $list: true
        }
      },
      matches: {
        $db: 'matchdb',
        rando: true,
        sport: {
          $db: 'default',
          rando: true
        },
        sports: {
          $db: 'default',
          rando: true,
          $list: true
        },
        $list: true
      }
    }),
    {
      rando: 'rando sport!',
      match: {
        rando: 'rando match!',
        sport: {
          rando: 'rando sport!'
        },
        sports: [
          {
            rando: 'rando sport!'
          }
        ]
      },
      matches: [
        {
          rando: 'rando match!',
          sport: {
            rando: 'rando sport!'
          },
          sports: [
            {
              rando: 'rando sport!'
            }
          ]
        }
      ]
    }
  )

  await client1.delete('root')
  await client1.destroy()
})

test.serial('$db with $list with filter and multiple', async t => {
  const client1 = connect({ port: port1 }, { loglevel: 'info' })

  await client1.set({
    $id: 'sp1',
    type: 'sport',
    rando: 'rando sport!',
    matches: ['ma1', 'ma2', 'ma3']
  })

  await client1.set({
    $db: 'matchdb',
    $id: 'ma1',
    type: 'match',
    value: 1,
    rando: 'rando match 1!'
  })

  await client1.set({
    $db: 'matchdb',
    $id: 'ma2',
    type: 'match',
    value: 2,
    rando: 'rando match 2!'
  })

  await client1.set({
    $db: 'matchdb',
    $id: 'ma3',
    type: 'match',
    value: 3,
    rando: 'rando match 3!'
  })

  t.deepEqualIgnoreOrder(
    await client1.get({
      $id: 'sp1',
      rando: true,
      matches: {
        $db: 'matchdb',
        rando: true,
        value: true,
        $list: {
          $sort: {
            $field: 'value',
            $order: 'asc'
          },
          $find: {
            $filter: [
              {
                $operator: '..',
                $field: 'value',
                $value: [2, 3]
              }
            ]
          }
        }
      }
    }),
    {
      rando: 'rando sport!',
      matches: [
        {
          rando: 'rando match 2!',
          value: 2
        },

        {
          rando: 'rando match 3!',
          value: 3
        }
      ]
    }
  )

  await client1.delete('root')
  await client1.destroy()
})

test.serial('$db with $find', async t => {
  const client1 = connect({ port: port1 }, { loglevel: 'info' })

  await client1.set({
    $id: 'sp1',
    type: 'sport',
    rando: 'rando sport!',
    matches: ['ma1', 'ma2', 'ma3']
  })

  await client1.set({
    $db: 'matchdb',
    $id: 'ma1',
    type: 'match',
    value: 1,
    rando: 'rando match 1!'
  })

  await client1.set({
    $db: 'matchdb',
    $id: 'ma2',
    type: 'match',
    value: 2,
    rando: 'rando match 2!'
  })

  await client1.set({
    $db: 'matchdb',
    $id: 'ma3',
    type: 'match',
    value: 3,
    rando: 'rando match 3!'
  })

  t.deepEqualIgnoreOrder(
    await client1.get({
      $id: 'sp1',
      rando: true,
      match: {
        $db: 'matchdb',
        rando: true,
        value: true,
        $find: {
          $traverse: 'matches', // comes from "main db"
          $filter: [
            {
              $operator: '=',
              $field: 'value',
              $value: 3
            }
          ]
        }
      }
    }),
    {
      rando: 'rando sport!',
      match: {
        rando: 'rando match 3!',
        value: 3
      }
    }
  )

  await client1.delete('root')
  await client1.destroy()
})

test.serial('$db with $list.$find.$find', async t => {
  const client1 = connect({ port: port1 }, { loglevel: 'info' })

  await client1.set({
    $id: 'sp1',
    type: 'sport',
    value: 1,
    rando: 'rando sport!',
    matches: ['ma1', 'ma2', 'ma3']
  })

  await client1.set({
    $db: 'matchdb',
    $id: 'ma1',
    type: 'match',
    value: 1,
    sports: ['sp1'],
    rando: 'rando match 1!'
  })

  await client1.set({
    $db: 'matchdb',
    $id: 'ma2',
    type: 'match',
    value: 2,
    rando: 'rando match 2!',
    sports: ['sp1']
  })

  await client1.set({
    $db: 'matchdb',
    $id: 'ma3',
    type: 'match',
    value: 3,
    sports: ['sp1'],
    rando: 'rando match 3!'
  })

  t.deepEqualIgnoreOrder(
    await client1.get({
      $id: 'sp1',
      rando: true,
      sports: {
        $db: 'matchdb',
        rando: true,
        value: true,
        $list: {
          $sort: {
            $field: 'value',
            $order: 'asc'
          },
          $find: {
            $traverse: 'matches',
            $filter: [
              {
                $operator: '..',
                $field: 'value',
                $value: [2, 3]
              }
            ],
            $find: {
              $traverse: 'sports',
              $db: 'default',
              $filter: [
                {
                  $operator: '=',
                  $field: 'value',
                  $value: 1
                }
              ]
            }
          }
        }
      }
    }),
    {
      rando: 'rando sport!',
      sports: [
        {
          rando: 'rando sport!',
          value: 1
        }
      ]
    }
  )

  await client1.delete('root')
  await client1.destroy()
})
