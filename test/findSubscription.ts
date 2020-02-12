import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'
import { wait, dumpDb } from './assertions'

let srv
test.before(async t => {
  srv = await start({
    port: 6122,
    developmentLogging: true,
    loglevel: 'info'
  })
  await wait(1500)
  const client = connect({ port: 6122 })
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
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
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          date: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      }
    }
  })
})

test.after(async _t => {
  const client = connect({ port: 6122 })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('subscription find', async t => {
  const client = connect({ port: 6122 })

  const matches = []
  const teams = []

  for (let i = 0; i < 100; i++) {
    teams.push({
      $id: await client.id({ type: 'team' }),
      name: 'team ' + i,
      type: 'team'
    })
  }

  for (let i = 0; i < 10; i++) {
    matches.push({
      name: 'match ' + i,
      type: 'match',
      value: i,
      parents: {
        $add: [
          teams[~~(Math.random() * teams.length)].$id,
          teams[~~(Math.random() * teams.length)].$id
        ]
      },
      status: i < 5 ? 100 : 300
    })
  }

  await Promise.all(teams.map(t => client.set(t)))

  await client.set({
    type: 'league',
    name: 'league 1',
    children: matches
  })

  // if not id id = root

  const result = await client.get({
    // add id as well
    $includeMeta: true,
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
            },
            {
              $field: 'value',
              $operator: '..',
              $value: [5, 10]
            }
          ]
        }
      }
    }
    // make deeper things as well
  })
  // after this nested stuff as well

  // collected for everything

  // start with deceandants
  // then ancestors
  // then fields

  /*
    [{
        // and in value
       member: [{ field: 'ancestors', value: ['root']}],
       time?: [213123, 31123] // if at this time,
       fields: {
            // type is handled special
            'type': [
                    {
                        $value: 'ma' // make it prefix allready
                        $operator: '='
                    }
            ],
            'value': [
                    $operator: '..',
                    $value: [5, 10],
            ]
       }
    }]
  */

  console.dir(result.$meta, { depth: 100 })

  t.true(true)
})
