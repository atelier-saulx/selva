import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  await wait(500)

  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    types: {
      team: {
        prefix: 'te',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('find - single', async t => {
  // simple nested - single query
  const client = connect({ port }, { loglevel: 'info' })

  const team = await client.set({
    $id: 'te0',
    type: 'team',
    name: 'team0'
  })
  const matches = []
  for (let i = 0; i < 11; i++) {
    matches.push({
      $id: await client.id({ type: 'match' }),
      type: 'match',
      name: 'match' + i,
      parents: [team]
    })
  }
  
  await Promise.all(matches.map(v => client.set(v)))

  const r = await client.get({
    $id: 'te0',
    matches: {
      name: true,
      $list: {
        $find: {
          $find: {
            $traverse: 'children',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match'
              }
            ]
          }
        }
      }
    }
  })
  console.log('r', r)
  t.fail()
  // const r = await client.get({
  //   $id: 'te0',
  //   singleMatch: {
  //     name: true,
  //     $find: {
  //       $traverse: 'children',
  //       $filter: [
  //         {
  //           $field: 'type',
  //           $operator: '=',
  //           $value: 'match'
  //         },
  //         {
  //           $field: 'name',
  //           $operator: '=',
  //           $value: 'match0'
  //         }
  //       ]
  //     }
  //   }
  // })
  //
  // console.log('>>', r)
  // t.deepEqual(r, {
  //   singleMatch: { name: 'match0' }
  // })
})

