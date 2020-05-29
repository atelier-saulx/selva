import test from 'ava'
import { connect } from '../src/index'
import { start, startOrigin } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let srv2
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({ port })

  srv2 = await startOrigin({ name: 'users', registry: { port } })

  const client = connect({ port })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      custom: {
        prefix: 'cu',
        fields: {
          value: { type: 'number', search: true },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      }
    }
  })

  await client.updateSchema(
    {
      languages: ['en', 'de', 'nl'],
      types: {
        user: {
          prefix: 'us',
          fields: {
            value: { type: 'number', search: true },
            age: { type: 'number' },
            auth: {
              type: 'json'
            },
            title: { type: 'text' },
            description: { type: 'text' },
            image: {
              type: 'object',
              properties: {
                thumb: { type: 'string' },
                poster: { type: 'string' }
              }
            }
          }
        }
      }
    },
    'users'
  )

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('get - multi db', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $id: 'root',
    children: [
      {
        $id: 'cu1',
        title: { en: 'some event' }
      }
    ]
  })

  await client.set({
    $db: 'users',
    $id: 'root',
    children: [
      {
        $id: 'us1',
        name: 'mr ball',
        title: { en: 'ballzman' }
      }
    ]
  })

  const x = {
    component: {
      $value: 'List'
    },
    title: {
      $value: 'Players'
    },
    $db: 'users',
    $id: 'root',
    children: {
      id: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: {
            $value: 'user',
            $field: 'type',
            $operator: '='
          }
        }
      },
      title: {
        $field: 'name'
      }
    }
  }

  const y = await client.get(x)

  t.is(y.children.length, 1, 'simple multi db')

  const x2 = {
    component: {
      $value: 'List'
    },
    title: {
      $value: 'Players'
    },
    $db: 'users',
    children: {
      id: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: {
            $value: 'user',
            $field: 'type',
            $operator: '='
          }
        }
      },
      title: {
        $field: 'name'
      }
    }
  }

  const y2 = await client.get(x2)

  t.is(y2.children.length, 1, 'simple multi db without specifying id')

  const xz = {
    component: {
      $value: 'List'
    },
    title: {
      $value: 'Players'
    },
    // have to be able to omit id here
    children: {
      id: true,
      $db: 'users',
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: {
            $value: 'user',
            $field: 'type',
            $operator: '='
          }
        }
      },
      title: {
        $field: 'name'
      }
    }
  }

  const yz = await client.get(xz)

  t.is(yz.children.length, 1, 'putting db in children field')

  const x3 = {
    components: [
      {
        component: {
          $value: 'List'
        },
        title: {
          $value: 'Players'
        },
        // have to be able to omit id here
        $db: 'users',
        $id: 'root',
        children: {
          id: true,
          $list: {
            $find: {
              $traverse: 'descendants',
              $filter: {
                $value: 'user',
                $field: 'type',
                $operator: '='
              }
            }
          },
          title: {
            $field: 'name'
          }
        }
      }
    ]
  }

  const y3 = await client.get(x3)

  t.is(y3.components[0].children.length, 1, 'using array multi db with id')

  const x4 = {
    components: [
      {
        component: {
          $value: 'List'
        },
        title: {
          $value: 'Players'
        },
        // have to be able to omit id here
        $db: 'users',
        children: {
          id: true,
          $list: {
            $find: {
              $traverse: 'descendants',
              $filter: {
                $value: 'user',
                $field: 'type',
                $operator: '='
              }
            }
          },
          title: {
            $field: 'name'
          }
        }
      }
    ]
  }

  const y4 = await client.get(x3)

  t.is(
    y4.components[0].children.length,
    1,
    'using array multi db without specifying id'
  )

  const x5 = {
    components: [
      {
        component: {
          $value: 'List'
        },
        title: {
          $value: 'Players'
        },
        children: {
          $db: 'users',
          id: true,
          $list: {
            $find: {
              $traverse: 'descendants',
              $filter: {
                $value: 'user',
                $field: 'type',
                $operator: '='
              }
            }
          },
          title: {
            $field: 'name'
          }
        }
      }
    ]
  }

  const y5 = await client.get(x5)

  t.is(
    y5.components[0].children.length,
    1,
    'using array multi db without id and putting field in children directly'
  )
})
