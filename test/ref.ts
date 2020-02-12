import test from 'ava'
import { connect } from '../client/src/index'
import { start } from '../server/src/index'
import './assertions'

let srv
test.before(async t => {
  srv = await start({
    port: 7073
  })

  const client = connect({ port: 7073 })
  await client.updateSchema({
    languages: ['en', 'en_us', 'en_uk', 'de', 'nl'],
    types: {
      lekkerType: {
        prefix: 'vi',
        fields: {
          name: { type: 'string' },
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
  const client = connect({ port: 7073 })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('string field ref', async t => {
  const client = connect({ port: 7073 })

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

  client.destroy()
})

test.serial('json field ref', async t => {
  const client = connect({ port: 7073 })

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
  client.destroy()
})

test.serial('whole object ref', async t => {
  const client = connect({ port: 7073 })

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
  client.destroy()
})

test.serial('simple object field ref', async t => {
  const client = connect({ port: 7073 })

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
  client.destroy()
})

test.serial('nested object in object field ref', async t => {
  const client = connect({ port: 7073 })

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
  client.destroy()
})

test.serial('text field ref', async t => {
  const client = connect({ port: 7073 })

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
  client.destroy()
})

test.serial('text object ref', async t => {
  const client = connect({ port: 7073 })

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
  client.destroy()
})

test.serial('string field ref with $default', async t => {
  const client = connect({ port: 7073 })

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

  client.destroy()
})

test.serial('number field ref with $default', async t => {
  const client = connect({ port: 7073 })

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

  client.destroy()
})

test.serial('object nested field ref with $default', async t => {
  const client = connect({ port: 7073 })

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

  client.destroy()
})

test.serial('text field ref with $default', async t => {
  const client = connect({ port: 7073 })

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

  client.destroy()
})
