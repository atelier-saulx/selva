import test from 'ava'
import { connect } from '../src/index'
import { start, startOrigin } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'
import { FieldSchemaArrayLike } from '../src/schema'

let srv1
let srv2
let port1: number
test.before(async t => {
  port1 = await getPort()
  srv1 = await start({
    port: port1
  })
  const client = connect({ port: port1 })
  await client.updateSchema({
    languages: ['en'],
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' }
        }
      }
    }
  })

  srv2 = await startOrigin({
    name: 'users',
    registry: { port: port1 }
  })

  await client.updateSchema(
    {
      languages: ['en'],
      types: {
        watching: {
          prefix: 'wa',
          fields: {
            item: { type: 'reference' },
            time: { type: 'number' }
          }
        },
        user: {
          prefix: 'us',
          fields: {
            watching: { type: 'references' },
            favorites: { type: 'references' }
          }
        }
      }
    },
    'users'
  )

  await client.destroy()
})

test.after(async _t => {
  let client = connect({ port: port1 })
  let d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await srv1.destroy()

  d = Date.now()
  await client.delete({ $id: 'root', $db: 'matchdb' })
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv2.destroy()
})

test.serial('admin deletes', async t => {
  const client = connect({ port: port1 }, { loglevel: 'info' })

  await client.set({
    $id: 'root',
    children: {
      $add: [
        {
          $id: 'ma1',
          title: {
            en: 'yesh match 1'
          }
        },
        {
          $id: 'ma2',
          title: {
            en: 'yesh match 2'
          }
        }
      ]
    }
  })

  await client.set({
    $db: 'users',
    $id: 'root',
    children: {
      $add: [
        {
          $id: 'wa1',
          item: 'ma1',
          time: 1
        },
        {
          $id: 'wa2',
          item: 'ma2',
          time: 12
        },
        {
          $id: 'wa3',
          item: 'ma1',
          time: 77
        }
      ]
    }
  })

  await client.set({
    $db: 'users',
    $id: 'us1',
    favorites: {
      $add: 'ma1'
    },
    watching: {
      $add: ['wa1', 'wa2']
    }
  })

  await client.set({
    $db: 'users',
    $id: 'us2',
    favorites: {
      $add: ['ma1', 'ma2']
    },
    watching: {
      $add: ['wa3']
    }
  })

  let firstSchema = await client.getSchema()
  let firstKeys = await client.redis.keys('*')
  await client.deleteType('match')
  delete firstSchema.schema.types.match
  t.deepEqualIgnoreOrder(
    firstSchema.schema.types,
    (await client.getSchema()).schema.types
  )

  for (let i = 0; i < firstKeys.length; i++) {
    if (firstKeys[i].startsWith('ma')) {
      const exists = await client.redis.exists(firstKeys[i])
      if (exists) {
        console.error(firstKeys[i], exists)
        t.fail()
      }
    }
  }

  await client.deleteField('watching', 'time', 'users')

  firstSchema = await client.getSchema('users')
  firstKeys = await client.redis.keys({ name: 'users' }, '*')
  await client.deleteField('watching', 'time', 'users')
  delete firstSchema.schema.types.watching.fields.time
  t.deepEqualIgnoreOrder(
    firstSchema.schema.types,
    (await client.getSchema('users')).schema.types
  )

  console.log('first keys', firstKeys)
  for (let i = 0; i < firstKeys.length; i++) {
    if (firstKeys[i].startsWith('wa') && firstKeys[i].indexOf('.') === -1) {
      const hkeys = await client.redis.hkeys({ name: 'users' }, firstKeys[i])
      console.log('hkeys', hkeys)
      if (hkeys.includes('time')) {
        t.fail()
      }
    }
  }

  await client.castField(
    'user',
    'favorites',
    <FieldSchemaArrayLike>{ type: 'set', items: { type: 'string' } },
    'users'
  )
  console.log(
    'HMM YES',
    JSON.stringify(await client.getSchema('users'), null, 2)
  )

  await client.delete('root')
  await client.destroy()
})
