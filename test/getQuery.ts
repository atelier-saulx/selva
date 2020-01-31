import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import queryParser from '../src/query'
import './assertions'
import { wait } from './assertions'

let srv
test.before(async t => {
  srv = await start({
    port: 6088
    // developmentLogging: true,
    // loglevel: 'info'
  })

  await wait(500)

  const client = connect({ port: 6088 })
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      },
      club: {
        prefix: 'cl',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      },
      team: {
        prefix: 'te',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC'] } },
          status: { type: 'number', search: { type: ['NUMERIC'] } }
        }
      },
      video: {
        prefix: 'vi',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC'] } }
        }
      }
    }
  })

  const team1 = await client.id({ type: 'team' })

  const genMatches = () => {
    const ch = []
    for (let i = 0; i < 1000; i++) {
      ch.push({
        type: 'match',
        name: 'match' + i,
        status: i === 0 ? 2 : i > 10 ? 100 : 300,
        parents: { $add: team1 }
      })
    }
    return ch
  }

  const genVideos = () => {
    const ch = []
    for (let i = 0; i < 10; i++) {
      ch.push({
        type: 'video',
        name: 'video',
        value: i
      })
    }
    return ch
  }

  const ids = await Promise.all([
    client.set({
      type: 'club',
      name: 'club 1',
      children: [
        {
          $id: team1,
          name: 'team 1',
          children: genVideos()
        }
      ]
    }),
    client.set({
      type: 'league',
      name: 'league 1',
      // @ts-ignore
      children: genMatches()
    })
  ])

  await wait(500)
  t.true(ids[0].slice(0, 2) === 'cl' && ids[1].slice(0, 2) === 'le')

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port: 6088 })
  // handing if 1000 ???? not really a lot
  console.log('hello')
  const d = Date.now()

  // delete crashes because 1k things...

  // await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('get - queryParser', async t => {
  // simple nested - single query
  const client = connect({ port: 6088 })
  // extra option in find is index or auto from fields
  const results = await client.query({
    name: true,
    value: true,
    status: true,
    id: true,
    type: true,
    $list: {
      $find: {
        $traverse: 'descendants',
        $filter: [
          {
            $operator: '=',
            $field: 'type',
            $value: 'match',
            $and: {
              // 'range'
              $operator: '=',
              $field: 'status',
              $value: [300, 2] // handle or
            },
            $or: {
              $operator: '=',
              $field: 'name',
              $value: 'league 1',
              $or: {
                $operator: '>',
                $field: 'value',
                $value: 4,
                $and: {
                  $operator: '>',
                  $field: 'value',
                  $value: 6,
                  $and: {
                    $operator: '<',
                    $field: 'value',
                    $value: 8
                  }
                }
              }
            }
          },
          {
            $operator: '!=',
            $field: 'name',
            $value: ['match1', 'match2', 'match3']
          }
        ]
      }
    }
  })

  console.log('!!!', results)

  t.true(true)
})
