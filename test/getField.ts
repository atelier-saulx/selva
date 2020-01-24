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

  t.deepEqual(
    await client.get({
      $id: 'viB',
      id: true,
      somethingWithVariable: {
        $field: '${type}.thingydingy'
      },
      value: true
    }),
    {
      id: 'viB',
      somethingWithVariable: 'Thing-y Ding-y',
      value: 25
    }
  )
})

test.serial('get - alias with nested structure variable', async t => {
  const client = connect({ port: 7072 })

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

  t.deepEqual(
    await client.get({
      $id: 'viC',
      id: true,
      nestedFun: {
        $field: '${title.en}.ecin'
      },
      value: true
    }),
    {
      id: 'viC',
      nestedFun: 'lekker man, het werkt',
      value: 25
    }
  )
})

test.serial('get - alias with variables', async t => {
  const client = connect({ port: 7072 })

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

  t.deepEqual(
    await client.get({
      $id: 'viD',
      id: true,
      niceFromJson: {
        $field: '${title.en}.complexNice.${type}.superSecret'
      },
      value: true
    }),
    {
      id: 'viD',
      niceFromJson: 'yesh!',
      value: 25
    }
  )
})

test.serial('get - $field with multiple options, taking the first', async t => {
  const client = connect({ port: 7072 })

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

  t.deepEqual(
    await client.get({
      $id: 'viE',
      id: true,
      valueOrAge: { $field: ['value', 'age'] }
    }),
    {
      id: 'viE',
      valueOrAge: 25
    }
  )
})

test.serial(
  'get - $field with multiple options, taking the second',
  async t => {
    const client = connect({ port: 7072 })

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

    t.deepEqual(
      await client.get({
        $id: 'viF',
        id: true,
        valueOrAge: { $field: ['value', 'age'] }
      }),
      {
        id: 'viF',
        valueOrAge: 62
      }
    )
  }
)

test.serial(
  'get - $field with multiple options complex. taking the second',
  async t => {
    const client = connect({ port: 7072 })

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

    t.deepEqual(
      await client.get({
        $id: 'viG',
        id: true,
        complexOr: {
          $field: [
            '${title.en}.complexNice.${type}.superSecret',
            '${type}.thingydingy'
          ]
        }
      }),
      {
        id: 'viG',
        complexOr: 'Thing-y Ding-y'
      }
    )
  }
)
