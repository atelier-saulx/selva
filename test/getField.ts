import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'

test.before(async t => {
  await start({
    port: 7072,
    developmentLogging: true,
    loglevel: 'info'
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })

  const client = connect({ port: 7072 })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      lekkerType: {
        prefix: 'vi',
        fields: {
          nice: {
            type: 'object',
            properties: {
              ecin: { type: 'string' },
              complexNice: {
                type: 'object',
                properties: { lekkerType: { type: 'json' } }
              }
            }
          },
          lekkerType: {
            type: 'object',
            properties: { thingydingy: { type: 'string' } }
          },
          thing: { type: 'set', items: { type: 'string' } },
          ding: {
            type: 'object',
            properties: {
              dong: { type: 'set', items: { type: 'string' } }
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
          name: { type: 'string' },
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
          description: { type: 'text' }
        }
      }
    }
  })

  await client.destroy()
})

test.serial('get - simple alias', async t => {
  const client = connect({ port: 7072 })

  await client.set({
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
  })

  t.deepEqual(
    await client.get({
      $id: 'viA',
      id: true,
      enTitle: {
        $field: 'title.en'
      },
      value: true
    }),
    {
      id: 'viA',
      enTitle: 'nice!',
      value: 25
    }
  )
})

test.serial('get - simple alias with variable', async t => {
  const client = connect({ port: 7072 })

  await client.set({
    $id: 'viA',
    title: {
      en: 'nice!'
    },
    lekkerType: {
      thingydingy: 'Thing-y Ding-y'
    },
    value: 25,
    auth: {
      // role needs to be different , different roles per scope should be possible
      role: {
        id: ['root'],
        type: 'admin'
      }
    }
  })

  t.deepEqual(
    await client.get({
      $id: 'viA',
      id: true,
      somethingWithVariable: {
        $field: '${type}.thingydingy'
      },
      value: true
    }),
    {
      id: 'viA',
      somethingWithVariable: 'Thing-y Ding-y',
      value: 25
    }
  )
})

test.serial('get - alias with nested structure variable', async t => {
  const client = connect({ port: 7072 })

  await client.set({
    $id: 'viA',
    title: {
      en: 'nice'
    },
    nice: {
      ecin: 'lekker man, het werkt'
    },
    value: 25,
    auth: {
      // role needs to be different , different roles per scope should be possible
      role: {
        id: ['root'],
        type: 'admin'
      }
    }
  })

  t.deepEqual(
    await client.get({
      $id: 'viA',
      id: true,
      nestedFun: {
        $field: '${title.en}.ecin'
      },
      value: true
    }),
    {
      id: 'viA',
      nestedFun: 'lekker man, het werkt',
      value: 25
    }
  )
})

test.serial('get - alias with variables', async t => {
  const client = connect({ port: 7072 })

  await client.set({
    $id: 'viA',
    title: {
      en: 'nice'
    },
    nice: {
      ecin: 'lekker man, het werkt',
      complexNice: { lekkerType: { superSecret: 'yesh!' } }
    },
    value: 25,
    auth: {
      // role needs to be different , different roles per scope should be possible
      role: {
        id: ['root'],
        type: 'admin'
      }
    }
  })

  t.deepEqual(
    await client.get({
      $id: 'viA',
      id: true,
      niceFromJson: {
        $field: '${title.en}.complexNice.${type}.superSecret'
      },
      value: true
    }),
    {
      id: 'viA',
      niceFromJson: 'yesh!',
      value: 25
    }
  )
})
