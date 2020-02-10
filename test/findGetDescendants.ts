import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'
import { wait, dumpDb } from './assertions'
import { RedisClient } from 'redis'
import { collapseTextChangeRangesAcrossMultipleVersions } from 'typescript'

let srv
test.before(async t => {
  srv = await start({
    port: 6088,
    developmentLogging: true,
    loglevel: 'info'
  })
  await wait(1500)
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
      match: {
        prefix: 'ma',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      }
    }
  })
})

test.serial('get decendants using get syntax', async t => {
  const client = connect({ port: 6088 })

  const matches = []
  for (let i = 0; i < 10; i++) {
    matches.push({
      name: 'match ' + i,
      type: 'match',
      value: i,
      status: i < 5 ? 100 : 300
    })
  }

  await client.set({
    type: 'league',
    name: 'league 1',
    children: matches
  })

  // if not id id = root
  const result = await client.get({
    items: {
      name: true,
      id: true,
      $list: {
        $find: {
          $traverse: 'descendants',
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
  })

  console.log('RESULT -->', result)

  t.true(true)
  await wait(1500)
})
