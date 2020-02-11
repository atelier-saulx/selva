import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'
import { wait } from './assertions'

let srv
test.before(async t => {
  srv = await start({ port: 6062, developmentLogging: true, loglevel: 'info' })
  await wait(500)
})

test.after(async _t => {
  const client = connect({ port: 6062 })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('get - simple $list', async t => {
  const client = connect({ port: 6062 })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      custom: {
        prefix: 'cu',
        fields: {
          name: { type: 'string' },
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

  const children = []

  for (let i = 0; i < 100; i++) {
    children.push({
      type: 'custom',
      value: i,
      name: 'flurp' + i
    })
  }

  await Promise.all([
    client.set({
      $id: 'cuA',
      image: {
        thumb: 'flurp.jpg'
      },
      title: { en: 'snurf' },
      children
    })
  ])

  const c = await client.get({
    $id: 'cuA',
    children: {
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $range: [0, 10]
      }
    }
  })

  t.deepEqual(
    c,
    {
      children: [
        { value: 0, name: 'flurp0' },
        { value: 1, name: 'flurp1' },
        { value: 2, name: 'flurp2' },
        { value: 3, name: 'flurp3' },
        { value: 4, name: 'flurp4' },
        { value: 5, name: 'flurp5' },
        { value: 6, name: 'flurp6' },
        { value: 7, name: 'flurp7' },
        { value: 8, name: 'flurp8' },
        { value: 9, name: 'flurp9' }
      ]
    },
    'non redis search sort'
  )

  const { children: rangeResult } = await client.get({
    $id: 'cuA',
    children: {
      name: true,
      value: true,
      $list: {
        $range: [0, 10]
      }
    }
  })

  t.is(rangeResult.length, 10, 'non redis search range')

  /*
  const x = await client.get({
    $id: 'cuA',
    related: {
      $inherit: true,
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $range: [0, 10]
      }
    }
  })
  */
})

test.serial('get - simple $list with $field of one field', async t => {
  const client = connect({ port: 6062 })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      custom: {
        prefix: 'cu',
        fields: {
          name: { type: 'string' },
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

  const children = []

  for (let i = 0; i < 100; i++) {
    children.push({
      type: 'custom',
      value: i,
      name: 'flurp' + i
    })
  }

  await Promise.all([
    client.set({
      $id: 'cuA',
      image: {
        thumb: 'flurp.jpg'
      },
      title: { en: 'snurf' },
      children
    })
  ])

  const c = await client.get({
    $id: 'cuA',
    otherName: {
      name: true,
      value: true,
      $field: 'children',
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $range: [0, 10]
      }
    }
  })

  t.deepEqual(
    c,
    {
      otherName: [
        { value: 0, name: 'flurp0' },
        { value: 1, name: 'flurp1' },
        { value: 2, name: 'flurp2' },
        { value: 3, name: 'flurp3' },
        { value: 4, name: 'flurp4' },
        { value: 5, name: 'flurp5' },
        { value: 6, name: 'flurp6' },
        { value: 7, name: 'flurp7' },
        { value: 8, name: 'flurp8' },
        { value: 9, name: 'flurp9' }
      ]
    },
    'non redis search sort'
  )
})

test.serial('get - simple $list with $field of two field entries', async t => {
  const client = connect({ port: 6062 })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      custom: {
        prefix: 'cu',
        fields: {
          name: { type: 'string' },
          value: { type: 'number', search: true },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          related: { type: 'references' },
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

  const children = []

  for (let i = 0; i < 100; i++) {
    children.push({
      type: 'custom',
      value: i,
      name: 'flurp' + i
    })
  }

  await Promise.all([
    client.set({
      $id: 'cuA',
      image: {
        thumb: 'flurp.jpg'
      },
      title: { en: 'snurf' },
      children
    })
  ])

  const c = await client.get({
    $id: 'cuA',
    otherName: {
      name: true,
      value: true,
      $field: ['related', 'children'],
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $range: [0, 10]
      }
    }
  })

  t.deepEqual(
    c,
    {
      otherName: [
        { value: 0, name: 'flurp0' },
        { value: 1, name: 'flurp1' },
        { value: 2, name: 'flurp2' },
        { value: 3, name: 'flurp3' },
        { value: 4, name: 'flurp4' },
        { value: 5, name: 'flurp5' },
        { value: 6, name: 'flurp6' },
        { value: 7, name: 'flurp7' },
        { value: 8, name: 'flurp8' },
        { value: 9, name: 'flurp9' }
      ]
    },
    'non redis search sort'
  )
})
