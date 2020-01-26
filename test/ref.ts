import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'

test.before(async t => {
  await start({
    port: 7073,
    developmentLogging: true,
    loglevel: 'info'
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
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
          blob: { type: 'json' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
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
})

test.serial.only('text field ref', async t => {
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
})
