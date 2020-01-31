import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import queryParser from '../src/query'
import './assertions'

let srv
test.before(async t => {
  srv = await start({
    port: 6072,
    developmentLogging: true,
    loglevel: 'info'
  })

  const client = connect({ port: 6072 })
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

  const ids = await Promise.all([
    client.set({
      type: 'league',
      name: 'league 1',
      children: [
        {
          type: 'match',
          name: 'match1',
          status: 300
        },
        {
          type: 'match',
          name: 'match2',
          status: 300
        },
        {
          type: 'match',
          name: 'match3',
          status: 100
        }
      ]
    }),
    client.set({
      type: 'club',
      name: 'club 1',
      children: [
        {
          type: 'team',
          name: 'team 1',
          children: [
            {
              type: 'video',
              name: 'suprise video'
            }
          ]
        }
      ]
    })
  ])

  console.log(ids)

  t.true(ids[0].slice(0, 2) === 'le' && ids[1].slice(0, 2) === 'cl')

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port: 6072 })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('get - queryParser', async t => {
  // simple nested - single query

  // extra option in find is index or auto from fields
  const simpleQuery = {
    title: true,
    $list: {
      $find: {
        $traverse: 'descendants',
        $filter: [
          {
            $operator: '!=',
            $field: 'type',
            $and: {
              $operator: '!=',
              $field: 'name',
              $value: ['match1', 'match3']
            },
            $value: 'match'
          },
          {
            $operator: '!=',
            $field: 'name',
            $value: ['match1', 'match2']
          },
          {
            $operator: '=',
            $field: 'status',
            $value: 300,
            $and: {
              $operator: '!=',
              $field: 'type',
              $value: 'video' //bit weird merge but ok
            }
          }
        ]
      }
    }
  }

  // console.log(queryParser(simpleQeury4, 'root'))

  t.true(true)
})
