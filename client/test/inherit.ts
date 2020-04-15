import test from 'ava'
import { connect } from '@saulx/selva'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
// @ts-ignore suppressing module can only be default-imported using the 'esModuleInterop' flag
import getPort from 'get-port'

let srv
let port

test.before(async t => {
  port = await getPort()
  srv = await start({ port })
  await wait(500)
  const client = connect({ port: port })
  await client.updateSchema({
    types: {
      genre: {
        prefix: 'ge',
        fields: {
          name: { type: 'string' },
          icon: { type: 'string' },
          fields: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              something: { type: 'string' }
            }
          }
        }
      },
      movie: {
        prefix: 'mo',
        fields: {
          title: { type: 'string' }
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

test.serial('simple', async t => {
  const client = connect({ port: port })

  const genre = await client.set({
    $id: 'geScifi',
    name: 'Sci-fi',
    icon: 'scifi.png'
  })

  await client.set({
    $id: 'moSoylentGreen',
    title: 'Soylent Green',
    parents: [genre]
  })

  const result = await client.get({
    $id: 'moSoylentGreen',
    icon: { $inherit: true }
  })

  t.true(result.icon === 'scifi.png')
})

test.serial('$all', async t => {
  const client = connect({ port: port })

  const genre = await client.set({
    $id: 'geA',
    fields: {
      name: 'hello'
    }
  })

  const genre2 = await client.set({
    $id: 'geB',
    parents: ['geA']
  })

  const result1 = await client.get({
    $id: 'geA',
    fields: {
      $all: true,
      $inherit: true
    }
  })

  t.deepEqual(result1.fields, { name: 'hello', something: '' }, 'from field')

  const result = await client.get({
    $id: 'geB',
    fields: {
      $all: true,
      $inherit: true
    }
  })

  console.log(result.fields)
  t.deepEqual(result.fields, { name: 'hello', something: '' }, 'inherit')
})

test.serial('$field + object', async t => {
  const client = connect({ port: port })

  const genre = await client.set({
    $id: 'geC',
    fields: {
      name: 'hello'
    }
  })

  const genre2 = await client.set({
    $id: 'geD',
    parents: ['geA']
  })

  const result1 = await client.get({
    $id: 'geC',
    flaprdol: {
      name: { $field: 'fields.name' },
      $inherit: true
    }
  })

  t.deepEqual(result1, { flaprdol: { name: 'hello' } }, 'get')

  const result = await client.get({
    $id: 'geD',
    flaprdol: {
      name: { $field: 'fields.name' },
      $inherit: true
    }
  })

  console.log('res', result)

  t.deepEqual(result, { flaprdol: { name: 'hello' } }, 'inherit')
})

test.serial('$field + object + all', async t => {
  const client = connect({ port: port })

  const genre = await client.set({
    $id: 'geC',
    fields: {
      name: 'hello'
    }
  })

  const genre2 = await client.set({
    $id: 'geD',
    parents: ['geA']
  })

  const result1 = await client.get({
    $id: 'geC',
    flaprdol: {
      $all: true,
      $field: 'fields',
      $inherit: true
    }
  })

  t.deepEqual(result1, { flaprdol: { name: 'hello', something: '' } }, 'get')

  const result = await client.get({
    $id: 'geD',
    flaprdol: {
      $all: true,
      $field: 'fields',
      $inherit: true
    }
  })

  console.log('res', result)

  t.deepEqual(result, { flaprdol: { name: 'hello', something: '' } }, 'inherit')
})
