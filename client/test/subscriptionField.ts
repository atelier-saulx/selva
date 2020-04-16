import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import getPort from 'get-port'
import { wait } from './assertions'

let srv
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })

  const client = connect({ port })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      lekkerType: {
        prefix: 'vi',
        fields: {
          name: { type: 'string' },
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

test.after(async _t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('subscribe - simple alias', async t => {
  const client = connect({ port })

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

  const obs = await client.observe({
    $id: 'viA',
    id: true,
    enTitle: {
      $field: 'title.en'
    },
    value: true
  })

  const results = []
  obs.subscribe(res => {
    results.push(res)
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viA',
      enTitle: 'nice!',
      value: 25
    }
  ])

  await client.set({
    $id: 'viA',
    title: {
      en: 'better!'
    }
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viA',
      enTitle: 'nice!',
      value: 25
    },
    {
      id: 'viA',
      enTitle: 'better!',
      value: 25
    }
  ])
})

test.serial('subscribe - simple alias with variable', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viB',
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

  const results = []
  const obs = await client.observe({
    $id: 'viB',
    id: true,
    somethingWithVariable: {
      $field: '${type}.thingydingy'
    },
    value: true
  })

  const sub = obs.subscribe(res => {
    results.push(res)
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viB',
      somethingWithVariable: 'Thing-y Ding-y',
      value: 25
    }
  ])

  await client.set({
    $id: 'viB',
    lekkerType: {
      thingydingy: 'Thing-y Ding-y Wingy Slingslong'
    }
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viB',
      somethingWithVariable: 'Thing-y Ding-y',
      value: 25
    },
    {
      id: 'viB',
      somethingWithVariable: 'Thing-y Ding-y Wingy Slingslong',
      value: 25
    }
  ])

  sub.unsubscribe()
  await wait(1500)
})

test.serial('subscribe - alias with nested structure variable', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viC',
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

  const results = []
  const obs = await client.observe({
    $id: 'viC',
    id: true,
    nestedFun: {
      $field: '${title.en}.ecin'
    },
    value: true
  })

  obs.subscribe(res => {
    results.push(res)
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viC',
      nestedFun: 'lekker man, het werkt',
      value: 25
    }
  ])

  await client.set({
    $id: 'viC',
    nice: {
      ecin: 'lekker man, het werkt nog beter!'
    }
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viC',
      nestedFun: 'lekker man, het werkt',
      value: 25
    },
    {
      id: 'viC',
      nestedFun: 'lekker man, het werkt nog beter!',
      value: 25
    }
  ])
})

test.serial('subscribe - alias with variables', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viD',
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

  const results = []
  const obs = await client.observe({
    $id: 'viD',
    id: true,
    niceFromJson: {
      $field: '${title.en}.complexNice.${type}.superSecret'
    },
    value: true
  })

  obs.subscribe(res => {
    results.push(res)
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viD',
      niceFromJson: 'yesh!',
      value: 25
    }
  ])

  await client.set({
    $id: 'viD',
    nice: {
      complexNice: { lekkerType: { superSecret: 'wow!' } }
    }
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viD',
      niceFromJson: 'yesh!',
      value: 25
    },
    {
      id: 'viD',
      niceFromJson: 'wow!',
      value: 25
    }
  ])
})

test.serial(
  'subscribe - $field with multiple options, taking the first',
  async t => {
    const client = connect({ port })

    await client.set({
      $id: 'viE',
      title: {
        en: 'nice'
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

    const results = []
    const obs = await client.observe({
      $id: 'viE',
      id: true,
      valueOrAge: { $field: ['value', 'age'] }
    })

    obs.subscribe(res => {
      results.push(res)
    })

    await wait(500)
    t.deepEqual(results, [
      {
        id: 'viE',
        valueOrAge: 25
      }
    ])

    await client.set({
      $id: 'viE',
      value: 32
    })

    await wait(500)
    t.deepEqual(results, [
      {
        id: 'viE',
        valueOrAge: 25
      },
      {
        id: 'viE',
        valueOrAge: 32
      }
    ])
  }
)

test.serial(
  'subscribe - $field with multiple options, taking the second',
  async t => {
    const client = connect({ port })

    await client.set({
      $id: 'viF',
      title: {
        en: 'nice'
      },
      age: 62,
      auth: {
        // role needs to be different , different roles per scope should be possible
        role: {
          id: ['root'],
          type: 'admin'
        }
      }
    })

    const results = []
    const obs = await client.observe({
      $id: 'viF',
      id: true,
      valueOrAge: { $field: ['value', 'age'] }
    })

    obs.subscribe(res => {
      results.push(res)
    })

    await wait(500)
    t.deepEqual(results, [
      {
        id: 'viF',
        valueOrAge: 62
      }
    ])

    await client.set({
      $id: 'viF',
      value: 32
    })

    await wait(500)
    t.deepEqual(results, [
      {
        id: 'viF',
        valueOrAge: 62
      },
      {
        id: 'viF',
        valueOrAge: 32
      }
    ])
  }
)

test.serial(
  'subscribe - $field with multiple options complex. taking the second',
  async t => {
    const client = connect({ port })

    await client.set({
      $id: 'viG',
      title: {
        en: 'nice'
      },
      nice: { complexNice: {} },
      lekkerType: {
        thingydingy: 'Thing-y Ding-y'
      },
      age: 62,
      auth: {
        // role needs to be different , different roles per scope should be possible
        role: {
          id: ['root'],
          type: 'admin'
        }
      }
    })

    const results = []
    const obs = await client.observe({
      $id: 'viG',
      id: true,
      complexOr: {
        $field: [
          '${title.en}.complexNice.${type}.superSecret',
          '${type}.thingydingy'
        ]
      }
    })

    obs.subscribe(res => {
      results.push(res)
    })

    await wait(500)
    t.deepEqual(results, [
      {
        id: 'viG',
        complexOr: 'Thing-y Ding-y'
      }
    ])

    await client.set({
      $id: 'viG',
      lekkerType: {
        thingydingy: 'Thing-y Ding-y Wing Ding Dong'
      }
    })

    await wait(500)
    t.deepEqual(results, [
      {
        id: 'viG',
        complexOr: 'Thing-y Ding-y'
      },
      {
        id: 'viG',
        complexOr: 'Thing-y Ding-y Wing Ding Dong'
      }
    ])
  }
)

test.serial('get - simple $field with $inherit: true', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viH',
    title: {
      en: 'extranice',
      de: 'Ja, auf Deutsch'
    },
    nice: { complexNice: {} },
    lekkerType: {
      thingydingy: 'Thing-y Ding-y'
    },
    age: 62,
    auth: {
      // role needs to be different , different roles per scope should be possible
      role: {
        id: ['root'],
        type: 'admin'
      }
    }
  })

  await client.set({
    $id: 'viI',
    title: {
      en: 'nice'
    },
    parents: ['viH'],
    age: 62,
    auth: {
      // role needs to be different , different roles per scope should be possible
      role: {
        id: ['root'],
        type: 'admin'
      }
    }
  })

  const results = []
  const obs = await client.observe({
    $id: 'viI',
    id: true,
    germanTitle: {
      $field: 'title.de',
      $inherit: true
    }
  })

  obs.subscribe(res => {
    results.push(res)
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viI',
      germanTitle: 'Ja, auf Deutsch'
    }
  ])

  await client.set({
    $id: 'viH',
    title: {
      de: 'Oops, Nederlands'
    }
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viI',
      germanTitle: 'Ja, auf Deutsch'
    },
    {
      id: 'viI',
      germanTitle: 'Oops, Nederlands'
    }
  ])
})

test.serial('subscribe - simple $field with $inherit: $type', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'cuA',
    name: 'customA',
    title: {
      en: 'extraextranice',
      de: 'Ja, auf Deutsch 2'
    }
  })

  await client.set({
    $id: 'viJ',
    name: 'lekkerJ',
    title: {
      en: 'extranice',
      de: 'Ja, auf Deutsch'
    },
    parents: ['cuA']
  })

  await client.set({
    $id: 'viK',
    title: {
      en: 'nice'
    },
    parents: ['viJ'],
    age: 62,
    auth: {
      // role needs to be different , different roles per scope should be possible
      role: {
        id: ['root'],
        type: 'admin'
      }
    }
  })

  const results = []
  const obs = await client.observe({
    $id: 'viK',
    id: true,
    germanTitle: {
      $field: 'title.de',
      $inherit: { $type: 'custom' }
    }
  })

  obs.subscribe(res => {
    results.push(res)
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viK',
      germanTitle: 'Ja, auf Deutsch 2'
    }
  ])

  await client.set({
    $id: 'cuA',
    title: {
      de: 'Oops, Nederlands!'
    }
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viK',
      germanTitle: 'Ja, auf Deutsch 2'
    },
    {
      id: 'viK',
      germanTitle: 'Oops, Nederlands!'
    }
  ])
})

test.serial('subscribe - more complex $field with $inherit: $name', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'cuB',
    name: 'customB',
    title: {
      de: 'Ja, auf Deutsch 2'
    },
    image: {
      thumb: 'parent'
    }
  })

  await client.set({
    $id: 'viL',
    name: 'lekkerL',
    title: {
      de: 'Ja, auf Deutsch'
    },
    image: {
      thumb: 'child'
    },
    parents: ['cuB']
  })

  await client.set({
    $id: 'viM',
    title: {
      en: 'image'
    },
    parents: ['viL'],
    age: 62
  })

  const results = []
  const obs = await client.observe({
    $id: 'viM',
    id: true,
    thumby: {
      $field: '${title.en}.thumb',
      $inherit: { $name: 'customB' }
    }
  })

  obs.subscribe(res => {
    results.push(res)
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viM',
      thumby: 'parent'
    }
  ])

  await client.set({
    $id: 'cuB',
    image: {
      thumb: 'parent update!'
    }
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viM',
      thumby: 'parent'
    },
    {
      id: 'viM',
      thumby: 'parent update!'
    }
  ])
})

test.serial(
  'subscribe - query $field with multiple options complex. taking the second',
  async t => {
    const client = connect({ port }, { loglevel: 'info' })

    await client.set({
      $id: 'viG',
      title: {
        en: 'nice'
      },
      nice: { complexNice: {} },
      lekkerType: {
        thingydingy: 'title.en'
      },
      age: 62,
      auth: {
        // role needs to be different , different roles per scope should be possible
        role: {
          id: ['root'],
          type: 'admin'
        }
      }
    })

    await client.set({
      $id: 'viZ',
      title: {
        en: 'best'
      },
      nice: { complexNice: {} },
      lekkerType: {
        thingydingy: 'Thing-y Ding-y'
      },
      age: 62,
      auth: {
        // role needs to be different , different roles per scope should be possible
        role: {
          id: ['root'],
          type: 'admin'
        }
      }
    })

    const results = []
    const obs = await client.observe({
      $id: 'viG',
      id: true,
      complexThingy: {
        $field: {
          path: ['yes.no', '${lekkerType.thingydingy}'],
          value: {
            $id: 'viZ',
            title: {
              en: true
            },
            somethingElse: { $value: 'yes' }
          }
        }
      }
    })

    obs.subscribe(res => {
      results.push(res)
    })

    await wait(500)
    t.deepEqual(results, [
      {
        id: 'viG',
        complexThingy: 'best'
      }
    ])

    await client.set({
      $id: 'viZ',
      title: {
        en: 'even better'
      }
    })

    await wait(500)
    t.deepEqual(results, [
      {
        id: 'viG',
        complexThingy: 'best'
      },
      {
        id: 'viG',
        complexThingy: 'even better'
      }
    ])
  }
)

test.serial('subscribe - $field with object structure', async t => {
  const client = connect({ port }, { loglevel: 'info' })

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

  const results = []
  const obs = await client.observe({
    $id: 'viA',
    id: true,
    wrappingObject: {
      de: {
        $field: 'title.en'
      }
    },
    value: true
  })

  obs.subscribe(res => {
    results.push(res)
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viA',
      wrappingObject: {
        de: 'nice!'
      },
      value: 25
    }
  ])

  await client.set({
    $id: 'viA',
    title: {
      en: 'better!'
    }
  })

  await wait(500)
  t.deepEqual(results, [
    {
      id: 'viA',
      wrappingObject: {
        de: 'nice!'
      },
      value: 25
    },
    {
      id: 'viA',
      wrappingObject: {
        de: 'better!'
      },
      value: 25
    }
  ])
})
