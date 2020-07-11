import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
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

test.serial('get - simple alias', async t => {
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
  const client = connect({ port }, { loglevel: 'info' })

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

  t.deepEqual(
    await client.get({
      $id: 'viD',
      id: true,
      niceFromJson: {
        $field: '${title.en}.complexNice.${type}'
      },
      value: true
    }),
    {
      id: 'viD',
      niceFromJson: { superSecret: 'yesh!' },
      value: 25
    }
  )
})

test.serial('get - $field with multiple options, taking the first', async t => {
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

test.serial.skip('get - simple $field with $inherit: true', async t => {
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

  t.deepEqual(
    await client.get({
      $id: 'viI',
      id: true,
      germanTitle: {
        $field: 'title.de',
        $inherit: true
      }
    }),
    {
      id: 'viI',
      germanTitle: 'Ja, auf Deutsch'
    }
  )
})

test.serial.skip('get - simple $field with $inherit: $type', async t => {
  const client = connect({ port }, { loglevel: 'info' })

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

  t.deepEqual(
    await client.get({
      $id: 'viK',
      id: true,
      germanTitle: {
        $field: 'title.de',
        $inherit: { $type: 'custom' }
      }
    }),
    {
      id: 'viK',
      germanTitle: 'Ja, auf Deutsch 2'
    }
  )
})

test.serial.skip('get - more complex $field with $inherit: $name', async t => {
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

  t.deepEqual(
    await client.get({
      $id: 'viM',
      id: true,
      thumby: {
        $field: '${title.en}.thumb',
        $inherit: { $name: 'customB' }
      }
    }),
    {
      id: 'viM',
      thumby: 'parent'
    }
  )
})

test.serial(
  'get - query $field with multiple options complex. taking the second',
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

    t.deepEqual(
      await client.get({
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
      }),
      {
        id: 'viG',
        complexThingy: 'best'
      }
    )
  }
)

test.serial('get - $field with object structure', async t => {
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

  t.deepEqual(
    await client.get({
      $id: 'viA',
      id: true,
      wrappingObject: {
        de: {
          $field: 'title.de'
        }
      },
      value: true
    }),
    {
      id: 'viA',
      wrappingObject: {},
      value: 25
    }
  )
})

test.serial('get - nested query with $field in $id', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $id: 'viA',
    title: {
      en: 'viB'
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

  await client.set({
    $id: 'viB',
    title: {
      en: 'nice!!!'
    },
    value: 27
  })

  t.deepEqual(
    await client.get({
      $id: 'viA',
      id: true,
      thing: {
        $id: { $field: 'title.en' },
        title: true
      }
    }),
    {
      id: 'viA',
      thing: {
        title: {
          en: 'nice!!!'
        }
      }
    }
  )
})
