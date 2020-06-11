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
  const client = connect({ port: port1 })
  await client.updateSchema({
    languages: ['en'],
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text', search: { type: ['TEXT-LANGUAGE'] } }
        }
      }
    }
  })

  srv2 = await startOrigin({
    name: 'users',
    registry: { port: port1 }
  })

  await client.updateSchema(
    {
      languages: ['en'],
      types: {
        watching: {
          prefix: 'wa',
          fields: {
            item: { type: 'reference' },
            time: { type: 'number' }
          }
        },
        user: {
          prefix: 'us',
          fields: {
            watching: { type: 'references' },
            favorites: { type: 'references' }
          }
        }
      }
    },
    'users'
  )

  await client.destroy()
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

test.serial('make it nice with users', async t => {
  const client = connect({ port: port1 }, { loglevel: 'info' })

  await client.set({
    $id: 'root',
    children: {
      $add: [
        {
          $id: 'ma1',
          title: {
            en: 'yesh match 1'
          }
        },
        {
          $id: 'ma2',
          title: {
            en: 'yesh match 2'
          }
        }
      ]
    }
  })

  await client.set({
    $db: 'users',
    $id: 'root',
    children: {
      $add: [
        {
          $id: 'wa1',
          item: 'ma1',
          time: 1
        },
        {
          $id: 'wa2',
          item: 'ma2',
          time: 12
        },
        {
          $id: 'wa3',
          item: 'ma1',
          time: 77
        }
      ]
    }
  })

  await client.set({
    $db: 'users',
    $id: 'us1',
    favorites: {
      $add: 'ma1'
    },
    watching: {
      $add: ['wa1', 'wa2']
    }
  })

  await client.set({
    $db: 'users',
    $id: 'us2',
    favorites: {
      $add: ['ma1', 'ma2']
    },
    watching: {
      $add: ['wa3']
    }
  })

  try {
    console.log(
      'YEEEEEEEEEEES',
      JSON.stringify(
        await client.get({
          $includeMeta: true,
          $language: 'en',
          components: [
            {
              component: { $value: 'favorites' },
              $db: 'users',
              $id: 'us1',
              favorites: {
                $db: 'default',
                id: true,
                title: true,
                $list: true
              }
            },
            {
              component: { $value: 'watching' },
              $db: 'users',
              $id: 'us1',
              watching: {
                id: true,
                item: {
                  $db: 'default',
                  id: true,
                  title: true
                },
                $list: true
              }
            },
            {
              component: { $value: 'matches' },
              matches: {
                id: true,
                title: true,
                $list: {
                  $find: {
                    $traverse: 'descendants',
                    $filter: [
                      {
                        $operator: '=',
                        $field: 'type',
                        $value: 'match'
                      }
                    ]
                  }
                }
              }
            },
            {
              component: { $value: 'lolol' },
              things: {
                id: true,
                title: true,
                $list: {
                  $find: {
                    $traverse: {
                      $db: 'users',
                      $id: 'us2',
                      $field: 'favorites'
                    },
                    $filter: [
                      {
                        $operator: '=',
                        $field: 'title',
                        $value: 'match 2'
                      }
                    ]
                  }
                }
              }
            }
          ]
        }),
        null,
        2
      )
    )
  } catch (e) {
    console.error(e)
    t.fail()
  }

  t.pass()

  await client.delete('root')
  await client.destroy()
})
