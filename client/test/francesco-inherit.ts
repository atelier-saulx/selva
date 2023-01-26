import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
// @ts-ignore suppressing module can only be default-imported using the 'esModuleInterop' flag
import getPort from 'get-port'

let srv
let port

test.before(async (t) => {
  port = await getPort()
  srv = await start({ port })
  await wait(500)
  const client = connect({ port: port })
  await client.updateSchema({
    types: {
      flight: {
        prefix: 'fl',
        fields: {
          pilot: { type: 'reference' },
        },
      },
      pilot: {
        prefix: 'pi',
        fields: {
          name: { type: 'string' },
        },
      },
      file: {
        prefix: 'av',
        fields: {
          name: { type: 'string' },
          src: { type: 'string' },
          size: { type: 'number' },
        },
      },
      user: {
        prefix: 'us',
        fields: {
          email: { type: 'string' },
          avatar: {
            type: 'reference',
          },
        },
      },
    },
  })
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('simple', async (t) => {
  const client = connect({ port: port })

  const file = await client.set({
    type: 'file',
    name: 'avatarname',
    src: 'avatar.png',
    size: 100,
  })

  const pilot = await client.set({
    // $id: 'piPilot',
    type: 'pilot',
    name: 'snoopy',
  })
  const user = await client.set({
    type: 'user',
    email: 'derp@derp.com',
    avatar: file,
    children: { $add: pilot },
  })

  for (let i = 0; i < 5; i++) {
    await client.set({
      type: 'flight',
      pilot,
    })
  }

  const result = await client.get({
    flights: {
      id: true,
      pilot: {
        id: true,
        name: true,
        parents: true,
        avatar: {
          id: true,
          name: true,
          src: true,
          size: true,
          $inherit: { $type: ['user'] },
        },
        // firstName: { $inherit: { $type: ['user'] } },
        // lastName: { $inherit: { $type: ['user'] } },
      },
      $list: {
        $offset: 0,
        $limit: 10,
        $find: {
          $traverse: 'descendants',
          $filter: [{ $field: 'type', $operator: '=', $value: 'flight' }],
        },
      },
    },
  })

  // console.log(
  //   await client.get({ $id: user, $all: true, avatar: { $all: true } })
  // )
  // console.log(await client.get({ $id: file, $all: true }))
  // console.log(await client.get({ $id: pilot, $all: true, ancestors: true }))

  // console.log(JSON.stringify(result.flights[0], null, 2))

  t.deepEqual(result.flights[0].pilot.avatar.name, 'avatarname')

  await client.destroy()
})
