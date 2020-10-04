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
    languages: ['en', 'en_us', 'en_uk', 'de', 'nl'],
    types: {
      lekkerType: {
        prefix: 'vi',
        fields: {
          strVal: { type: 'string' },
          value: { type: 'number' },
          age: { type: 'number' },
          price: { type: 'number' },
          blob: { type: 'json' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          textThing: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
              yesh: {
                type: 'object',
                properties: {
                  test: { type: 'string' }
                }
              }
            }
          },
          yesh: {
            type: 'object',
            properties: {
              test: { type: 'string' }
            }
          },
          test: {
            type: 'object',
            properties: {
              test: { type: 'string' }
            }
          },
          thumb: { type: 'string' },
          favicon: {
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
          description: { type: 'text' }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.skip('string field ref', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viA',
    name: 'yesh',
    title: {
      en: 'nice!'
    },
    strVal: { $ref: 'name' },
    value: 25
  })

  t.deepEqual(
    await client.get({
      $id: 'viA',
      id: true,
      value: true,
      name: true,
      strVal: true
    }),
    {
      id: 'viA',
      value: 25,
      name: 'yesh',
      strVal: 'yesh'
    }
  )

  await client.destroy()
})

test.skip('json field ref', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viB',
    auth: {
      // role needs to be different , different roles per scope should be possible
      role: {
        id: ['root'],
        type: 'admin'
      }
    },
    blob: { $ref: 'auth' },
    value: 25
  })

  t.deepEqual(
    await client.get({
      $id: 'viB',
      id: true,
      value: true,
      blob: true
    }),
    {
      id: 'viB',
      value: 25,
      blob: {
        // role needs to be different , different roles per scope should be possible
        role: {
          id: ['root'],
          type: 'admin'
        }
      }
    }
  )
  await client.destroy()
})

test.skip('whole object ref', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viC',
    image: {
      thumb: 'thumbs up'
    },
    favicon: { $ref: 'image' },
    value: 25
  })

  t.deepEqual(
    await client.get({
      $id: 'viC',
      id: true,
      value: true,
      favicon: true
    }),
    {
      id: 'viC',
      favicon: {
        thumb: 'thumbs up'
      },
      value: 25
    }
  )
  await client.destroy()
})

test.skip('simple object field ref', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viC',
    image: {
      thumb: 'thumbs up'
    },
    thumb: { $ref: 'image.thumb' },
    value: 25
  })

  t.deepEqual(
    await client.get({
      $id: 'viC',
      id: true,
      value: true,
      thumb: true
    }),
    {
      id: 'viC',
      thumb: 'thumbs up',
      value: 25
    }
  )
  await client.destroy()
})

test.skip('nested object in object field ref', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viX',
    image: {
      yesh: { test: 'testytest' }
    },
    yesh: { $ref: 'image.yesh' },
    value: 25
  })

  t.deepEqual(
    await client.get({
      $id: 'viX',
      id: true,
      value: true,
      image: true,
      yesh: true
    }),
    {
      id: 'viX',
      image: {
        yesh: { test: 'testytest' }
      },
      yesh: { test: 'testytest' },
      value: 25
    }
  )
  await client.destroy()
})

test.skip('text field ref', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viD',
    title: {
      en: 'nice!',
      en_uk: { $ref: 'title.en' },
      en_us: { $ref: 'title.en' }
    },
    value: 25
  })

  t.deepEqual(
    await client.get({
      $id: 'viD',
      id: true,
      value: true,
      title: true
    }),
    {
      id: 'viD',
      title: {
        en: 'nice!',
        en_uk: 'nice!',
        en_us: 'nice!'
      },
      value: 25
    }
  )
  await client.destroy()
})

test.skip('text object ref', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viD',
    title: {
      en: 'nice!',
      en_uk: { $ref: 'title.en' },
      en_us: { $ref: 'title.en' }
    },
    description: { $ref: 'title' },
    value: 25
  })

  t.deepEqual(
    await client.get({
      $id: 'viD',
      id: true,
      value: true,
      title: true,
      description: true
    }),
    {
      id: 'viD',
      title: {
        en: 'nice!',
        en_uk: 'nice!',
        en_us: 'nice!'
      },
      description: {
        en: 'nice!',
        en_uk: 'nice!',
        en_us: 'nice!'
      },
      value: 25
    }
  )
  await client.destroy()
})

test.skip('string field ref with $default', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viE',
    name: 'yesh',
    title: {
      en: 'nice!'
    },
    strVal: { $default: { $ref: 'name' } },
    value: 25
  })

  t.deepEqual(
    await client.get({
      $id: 'viE',
      id: true,
      value: true,
      name: true,
      strVal: true
    }),
    {
      id: 'viE',
      value: 25,
      name: 'yesh',
      strVal: 'yesh'
    }
  )

  await client.set({
    $id: 'viE',
    name: 'yesh',
    title: {
      en: 'nice!'
    },
    strVal: { $default: { $ref: 'title.en' } },
    value: 25
  })

  t.deepEqual(
    await client.get({
      $id: 'viE',
      id: true,
      value: true,
      name: true,
      strVal: true
    }),
    {
      id: 'viE',
      value: 25,
      name: 'yesh',
      strVal: 'yesh'
    }
  )

  await client.destroy()
})

test.skip('number field ref with $default', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viF',
    name: 'yesh',
    age: 25,
    value: { $default: { $ref: 'age' } }
  })

  t.deepEqual(
    await client.get({
      $id: 'viF',
      id: true,
      value: true,
      age: true
    }),
    {
      id: 'viF',
      value: 25,
      age: 25
    }
  )

  await client.set({
    $id: 'viF',
    age: 25,
    price: 35,
    value: { $default: { $ref: 'price' } }
  })

  t.deepEqual(
    await client.get({
      $id: 'viF',
      id: true,
      value: true,
      age: true
    }),
    {
      id: 'viF',
      value: 25,
      age: 25
    }
  )

  await client.destroy()
})

test.skip('object nested field ref with $default', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viG',
    name: 'yes',
    image: {
      thumb: 'hello',
      poster: { $default: { $ref: 'image.thumb' } }
    }
  })

  t.deepEqual(
    await client.get({
      $id: 'viG',
      id: true,
      image: true
    }),
    {
      id: 'viG',
      image: {
        thumb: 'hello',
        poster: 'hello'
      }
    }
  )

  await client.set({
    $id: 'viG',
    image: {
      thumb: 'hello',
      poster: { $default: { $ref: 'name' } }
    }
  })

  t.deepEqual(
    await client.get({
      $id: 'viG',
      id: true,
      image: true
    }),
    {
      id: 'viG',
      image: {
        thumb: 'hello',
        poster: 'hello'
      }
    }
  )

  await client.destroy()
})

test.skip('text field ref with $default', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'viH',
    title: { nl: 'lekker title' },
    description: { nl: { $default: { $ref: 'title.nl' } } }
  })

  t.deepEqual(
    await client.get({
      $id: 'viH',
      id: true,
      description: true
    }),
    {
      id: 'viH',
      description: { nl: 'lekker title' }
    }
  )

  await client.set({
    $id: 'viH',
    textThing: { en: 'should not be this one' },
    description: { nl: { $default: { $ref: 'textThing.en' } } }
  })

  t.deepEqual(
    await client.get({
      $id: 'viH',
      id: true,
      description: true
    }),
    {
      id: 'viH',
      description: { nl: 'lekker title' }
    }
  )

  await client.destroy()
})
