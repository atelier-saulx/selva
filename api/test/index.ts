import test from 'ava'
import './assertions'
import { connect } from '@saulx/selva'
import { start } from '@saulx/selva-server'
import getPort from 'get-port'
import { start as apiStart } from '../src/index'
import { constructGuard, noHasGuard } from '../src/handler'
import fetch from 'node-fetch'

let srv
let port: number

test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })

  const client = connect({ port }, { loglevel: 'info' })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        value: { type: 'number' },
        nested: {
          type: 'object',
          properties: {
            fun: { type: 'string' }
          }
        }
      }
    },
    types: {
      lekkerType: {
        prefix: 'vi',
        fields: {
          thing: { type: 'set', items: { type: 'string' } },
          ding: {
            type: 'object',
            properties: {
              dong: { type: 'set', items: { type: 'string' } },
              dung: { type: 'number' },
              dang: {
                type: 'object',
                properties: {
                  dung: { type: 'number' },
                  dunk: { type: 'string' }
                }
              },
              dunk: {
                type: 'object',
                properties: {
                  ding: { type: 'number' },
                  dong: { type: 'number' }
                }
              }
            }
          },
          dong: { type: 'json' },
          dingdongs: { type: 'array', items: { type: 'string' } },
          refs: { type: 'references' },
          value: { type: 'number' },
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
      },
      custom: {
        prefix: 'cu',
        fields: {
          value: { type: 'number' },
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
      },
      club: {
        prefix: 'cl',
        fields: {
          value: { type: 'number' },
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
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          value: { type: 'number' },
          description: { type: 'text' },
          booly: { type: 'boolean' },
          recordy: {
            type: 'record',
            values: {
              type: 'object',
              properties: {
                a: { type: 'string' },
                b: { type: 'string' }
              }
            }
          },
          date: { type: 'timestamp' },
          start: { type: 'timestamp' },
          end: { type: 'timestamp' },
          status: { type: 'string' },
          published: { type: 'boolean' },
          homeTeam: { type: 'reference' },
          awayTeam: { type: 'reference' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' }
            }
          },
          video: {
            type: 'record',
            values: {
              type: 'object',
              properties: {
                hls: { type: 'string' },
                mp4: { type: 'string' }
              }
            }
          }
        }
      },
      yesno: {
        prefix: 'yn',
        fields: {
          bolYes: { type: 'boolean' },
          bolNo: { type: 'boolean' }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('test api ping/pong', async t => {
  const srvPort = await getPort()
  const cleanup = apiStart({ port }, [], srvPort)

  const res = await fetch(`http://localhost:${srvPort}/ping`, {
    method: 'POST',
    body: 'hellooo'
  })

  const body = await res.text()
  t.is(body, 'hellooo')

  cleanup()
})

test.serial('get $value through api', async t => {
  const srvPort = await getPort()
  const cleanup = apiStart({ port }, [], srvPort)

  const res = await fetch(`http://localhost:${srvPort}/get`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      yesh: { $value: 'hello' }
    })
  })

  const body = await res.json()
  t.deepEqualIgnoreOrder(body, {
    $isNull: true,
    yesh: 'hello'
  })

  cleanup()
})

test.serial('set and get simple through api', async t => {
  const srvPort = await getPort()
  const cleanup = apiStart({ port }, [], srvPort)

  await fetch(`http://localhost:${srvPort}/set`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      $id: 'maMatch1',
      title: {
        en: 'yes en',
        de: 'ja de'
      }
    })
  })

  const res = await fetch(`http://localhost:${srvPort}/get`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      $id: 'maMatch1',
      $language: 'de',
      id: true,
      title: true
    })
  })

  const body = await res.json()
  t.deepEqualIgnoreOrder(body, {
    id: 'maMatch1',
    title: 'ja de'
  })

  cleanup()
})

test.serial('delete simple through api', async t => {
  const srvPort = await getPort()
  const cleanup = apiStart({ port }, [], srvPort)

  let res = await fetch(`http://localhost:${srvPort}/get`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      $id: 'maMatch1',
      $language: 'de',
      id: true,
      title: true
    })
  })

  let body = await res.json()
  t.deepEqualIgnoreOrder(body, {
    id: 'maMatch1',
    title: 'ja de'
  })

  res = await fetch(`http://localhost:${srvPort}/delete`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      $id: 'maMatch1'
    })
  })

  body = await res.json()
  t.deepEqualIgnoreOrder(body, {
    isRemoved: true
  })

  res = await fetch(`http://localhost:${srvPort}/get`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      $id: 'maMatch1',
      $language: 'de',
      id: true,
      title: true
    })
  })

  body = await res.json()
  t.deepEqualIgnoreOrder(body, {
    id: 'maMatch1',
    $isNull: true,
    title: ''
  })

  cleanup()
})

test.serial('new schema and entry through api', async t => {
  const srvPort = await getPort()
  const cleanup = apiStart({ port }, [], srvPort)

  let res = await fetch(`http://localhost:${srvPort}/update_schema`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      types: {
        yeshyeshyesh: {
          prefix: 'ye',
          fields: {
            hello: { type: 'string' }
          }
        }
      }
    })
  })

  console.log('res', await res.json())

  await fetch(`http://localhost:${srvPort}/set`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      $id: 'ye1',
      hello: 'friend'
    })
  })

  res = await fetch(`http://localhost:${srvPort}/get`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      $id: 'ye1',
      id: true,
      hello: true
    })
  })

  const body = await res.json()
  t.deepEqualIgnoreOrder(body, {
    id: 'ye1',
    hello: 'friend'
  })

  cleanup()
})

test.serial('non-post throws', async t => {
  const srvPort = await getPort()
  const cleanup = apiStart({ port }, [], srvPort)

  let res = await fetch(`http://localhost:${srvPort}/update_schema`, {
    method: 'PUT',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      types: {
        yeshyeshyesh: {
          prefix: 'ye',
          fields: {
            hello: { type: 'string' }
          }
        }
      }
    })
  })

  t.is(res.status, 400)

  cleanup()
})

test.serial('test funky middleware', async t => {
  const srvPort = await getPort()
  const cleanup = apiStart(
    { port },
    [
      (_client, _req, res, next) => {
        res.statusCode = 418
        res.end("I'm a teapot")
        next(false)
      }
    ],
    srvPort
  )

  let res = await fetch(`http://localhost:${srvPort}/update_schema`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      types: {
        yeshyeshyesh: {
          prefix: 'ye',
          fields: {
            hello: { type: 'string' }
          }
        }
      }
    })
  })

  t.is(res.status, 418)
  t.is(await res.text(), "I'm a teapot")

  cleanup()
})

test.serial('test funky passing funky middleware', async t => {
  const srvPort = await getPort()
  const cleanup = apiStart(
    { port },
    [
      (_client, _req, res, next) => {
        res.setHeader('x-my-special-header', 'flurpy')
        next(true)
      }
    ],
    srvPort
  )

  let res = await fetch(
    `http://localhost:${srvPort}/update_schema?dbName=default`,
    {
      method: 'POST',
      headers: {
        'content-type': 'application/json'
      },
      body: JSON.stringify({
        types: {
          yeshyeshyesh: {
            prefix: 'ye',
            fields: {
              hello: { type: 'string' }
            }
          }
        }
      })
    }
  )

  t.is(res.headers.get('x-my-special-header'), 'flurpy')

  cleanup()
})

test.serial('test Guardy Guard', async t => {
  const simple = {
    $alias: 'hello',
    myString: 'hmmhmm'
  }

  t.deepEqual(constructGuard(simple), {
    $alias: 'hello',
    id: true,
    myString: true
  })
  t.deepEqual(true, noHasGuard(simple, { id: 'maYes', myString: 'hmmhmm' }))
  t.deepEqual(false, noHasGuard(simple, { id: 'maYes', myString: 'mmyes' }))

  const settingAliases = {
    $alias: 'hello',
    myString: 'hmmhmm',
    aliases: ['a', 'b', 'abba']
  }

  t.deepEqual(constructGuard(settingAliases), {
    $alias: 'hello',
    id: true,
    myString: true,
    aliases: true
  })
  t.deepEqual(
    true,
    noHasGuard(settingAliases, {
      id: 'maYes',
      myString: 'hmmhmm',
      aliases: ['a', 'b', 'abba']
    })
  )
  t.deepEqual(
    false,
    noHasGuard(settingAliases, {
      id: 'maYes',
      myString: 'hmmhmm',
      aliases: ['a', 'b']
    })
  )
  t.deepEqual(
    false,
    noHasGuard(settingAliases, {
      id: 'maYes',
      myString: 'hmmhmm',
      aliases: ['a', 'abba']
    })
  )

  const valueAliases = {
    $alias: 'hello',
    myString: 'hmmhmm',
    aliases: { $value: ['a', 'b', 'abba'] }
  }

  t.deepEqual(constructGuard(valueAliases), {
    $alias: 'hello',
    id: true,
    myString: true,
    aliases: true
  })
  t.deepEqual(
    true,
    noHasGuard(valueAliases, {
      id: 'maYes',
      myString: 'hmmhmm',
      aliases: ['a', 'b', 'abba']
    })
  )
  t.deepEqual(
    false,
    noHasGuard(valueAliases, {
      id: 'maYes',
      myString: 'hmmhmm',
      aliases: ['a', 'b']
    })
  )
  t.deepEqual(
    false,
    noHasGuard(valueAliases, {
      id: 'maYes',
      myString: 'hmmhmm',
      aliases: ['a', 'abba']
    })
  )

  const addingAliases = {
    $alias: 'hello',
    myString: 'hmmhmm',
    aliases: { $add: ['a', 'b'] }
  }

  t.deepEqual(constructGuard(addingAliases), {
    $alias: 'hello',
    id: true,
    myString: true,
    aliases: true
  })
  t.deepEqual(
    true,
    noHasGuard(addingAliases, {
      id: 'maYes',
      myString: 'hmmhmm',
      aliases: ['a', 'b', 'abba']
    })
  )
  t.deepEqual(
    false,
    noHasGuard(addingAliases, {
      id: 'maYes',
      myString: 'hmmhmm',
      aliases: ['abba', 'b']
    })
  )
  t.deepEqual(
    false,
    noHasGuard(addingAliases, {
      id: 'maYes',
      myString: 'hmmhmm',
      aliases: ['b', 'abba']
    })
  )

  const removingAliases = {
    $alias: 'hello',
    myString: 'hmmhmm',
    aliases: { $delete: ['a', 'b'] }
  }

  t.deepEqual(constructGuard(removingAliases), {
    $alias: 'hello',
    id: true,
    myString: true,
    aliases: true
  })
  t.deepEqual(
    true,
    noHasGuard(removingAliases, {
      id: 'maYes',
      myString: 'hmmhmm',
      aliases: ['abba']
    })
  )
  t.deepEqual(
    false,
    noHasGuard(removingAliases, {
      id: 'maYes',
      myString: 'hmmhmm',
      aliases: ['abba', 'b']
    })
  )
  t.deepEqual(
    false,
    noHasGuard(removingAliases, {
      id: 'maYes',
      myString: 'hmmhmm',
      aliases: ['b', 'abba']
    })
  )
})

test.serial.only('things', async t => {
  const srvPort = await getPort()
  const cleanup = apiStart({ port }, [], srvPort)

  await fetch(`http://localhost:${srvPort}/set`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      $alias: 'sas-ed52926f-9c23-45a7-a28a-10b00f38b34b',
      $language: 'de',
      type: 'match',
      title: 'HELIOS GRIZZLYS Giesen vs. SVG L�neburg',
      date: 1601744400000,
      start: 1601744400000,
      end: 1601753400000,
      status: '300',
      published: true,
      homeTeam: 'tebd06dfa1',
      awayTeam: 'teb1d47e94',
      image: {},
      video: {
        default: {
          hls:
            'https://dsmzkf3ry8xyv.cloudfront.net/out/v1/50e7271d570343498dc53b203c446659/index.m3u8',
          mp4:
            'https://download.sporttotal.tv/volleyball/germany/2020/10/5d1a2a13-d4f2-988b-9172-f52116af6d18/primary_vod.000000.ts'
        },
        pano: {}
      },
      parents: {
        $add: ['tebd06dfa1', 'teb1d47e94', 'coc70084e6']
      }
    })
  })

  await fetch(`http://localhost:${srvPort}/set`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      $alias: 'sas-ed52926f-9c23-45a7-a28a-10b00f38b34b',
      $language: 'de',
      type: 'match',
      title: 'HELIOS GRIZZLYS Giesen vs. SVG L�neburg',
      date: 1601744400000,
      start: 1601744400000,
      end: 1601753400000,
      status: '300',
      published: true,
      homeTeam: 'tebd06dfa1',
      awayTeam: 'teb1d47e94',
      image: {},
      video: {
        default: {
          hls:
            'https://dsmzkf3ry8xyv.cloudfront.net/out/v1/50e7271d570343498dc53b203c446659/index.m3u8',
          mp4:
            'https://download.sporttotal.tv/volleyball/germany/2020/10/5d1a2a13-d4f2-988b-9172-f52116af6d18/primary_vod.000000.ts'
        },
        pano: {}
      },
      parents: {
        $add: ['tebd06dfa1', 'teb1d47e94', 'coc70084e6']
      }
    })
  })

  const res = await fetch(`http://localhost:${srvPort}/get`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      $id: 'maMatch1',
      $language: 'en',
      id: true,
      title: true
    })
  })

  // const body = await res.json()
  // t.deepEqualIgnoreOrder(body, {
  //   id: 'maMatch1',
  //   title: 'yes en'
  // })

  cleanup()
})
