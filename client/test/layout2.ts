import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

/*
cms cases:
- create a layout
- edit a layout
- remove a layout
- update data related to layout
app cases:
- subscribe to layout for id
*/

let srv
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({ port })
  await wait(500)
  const client = connect({ port })
  const fields = {
    name: { type: 'string', search: { type: ['TAG'] } },
    title: { type: 'text', search: true },
    description: { type: 'text', search: true },
    start: {
      type: 'timestamp',
      search: { type: ['NUMERIC', 'SORTABLE'] }
    },
    end: {
      type: 'timestamp',
      search: { type: ['NUMERIC', 'SORTABLE'] }
    }
  }

  const types = {
    league: {
      prefix: 'le',
      fields
    },
    sport: {
      prefix: 'sp',
      fields
    },
    club: {
      prefix: 'cl',
      fields
    },
    team: {
      prefix: 'te',
      fields
    },
    match: {
      prefix: 'ma',
      fields
    }
  }

  // @ts-ignore
  fields.layout = {
    type: 'object',
    properties: [...Object.keys(types), 'root'].reduce((properties, type) => {
      properties[type] = {
        type: 'json'
      }
      return properties
    }, {})
  }

  await client.updateSchema({
    languages: ['en'],
    rootType: {
      // @ts-ignore
      fields
    },
    // @ts-ignore
    types: types
  })
})

test.after(async _t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('layout query', async t => {
  const client = connect({ port })
  // const client = connect({ port }, { loglevel: 'info' })

  // creating data
  const root = await client.set({
    $id: 'root',
    $language: 'en',
    layout: {
      match: {
        id: true,
        components: [
          {
            component: { $value: 'rootMatchComponent1' },
            title: {
              $field: 'title'
            },
            description: {
              $field: 'description'
            }
          }
        ]
      }
    }
  })

  const match = await client.set({
    type: 'match',
    $language: 'en',
    title: 'match one',
    description: 'match one story!',
    start: Date.now() - 10000,
    end: Date.now() + 60 * 60 * 1000 * 2
  })

  try {
    const inheritedMatchLayout = await client.get({
      $id: match,
      $language: 'en',
      ...(
        await client.get({
          $id: match,
          layout: { $inherit: true }
        })
      ).layout.match
    })
    t.is(
      inheritedMatchLayout.components[0].component,
      'rootMatchComponent1',
      'inherits from root'
    )
  } catch (e) {
    t.fail.skip('doesnt inherit root layout: ' + e.message)
  }

  // set new layout on match
  await client.set({
    $id: match,
    layout: {
      match: {
        id: true,
        components: [
          {
            component: { $value: 'matchMatchComponent1' },
            title: {
              $field: 'title'
            },
            description: {
              $field: 'description'
            }
          }
        ]
      }
    }
  })

  const matchLayout = await client.get({
    $id: match,
    $language: 'en',
    ...(
      await client.get({
        $id: match,
        layout: { $inherit: true }
      })
    ).layout.match
  })

  t.is(
    matchLayout.components[0].component,
    'matchMatchComponent1',
    'gets own layout'
  )

  // update layout on match
  await client.set({
    $id: match,
    layout: {
      match: {
        components: [
          {
            component: { $value: 'matchMatchComponent1Updated' }
          }
        ]
      }
    }
  })

  const matchLayoutUpdated = await client.get({
    $id: match,
    $language: 'en',
    ...(
      await client.get({
        $id: match,
        layout: { $inherit: true }
      })
    ).layout.match
  })

  t.is(
    matchLayoutUpdated.components[0].component,
    'matchMatchComponent1Updated',
    'gets own layout after update'
  )

  // subscribe to layout of match
  const obs = await client.observe({
    $id: match,
    layout: { $inherit: true }
  })

  // .then(obs => {
  //   obs.subscribe(res => {
  //     console.log('fires:', res)
  //   })
  // })

  // // change component name
  // await client.set({
  //   $id: match,
  //   layout: {
  //     match: {
  //       components: [
  //         {
  //           component: { $value: 'matchMatchComponent1Updated' }
  //         }
  //       ]
  //     }
  //   }
  // })

  // const team = await client.set({
  //   type: 'team',
  //   $language: 'en',
  //   title: 'team one',
  //   children: [match]
  // })

  // const teamLayout = await client.get({
  //   $id: team,
  //   $language: 'en',
  //   id: true,
  //   components: [
  //     {
  //       component: { $value: 'description' },
  //       title: {
  //         $field: 'title'
  //       },
  //       description: {
  //         $field: 'description'
  //       }
  //     },
  //     {
  //       component: { $value: 'gridLarge' },
  //       showall: { $value: true },
  //       children: {
  //         title: true,
  //         $list: {
  //           $range: [0, 100],
  //           $find: {
  //             $traverse: 'descendants',
  //             $filter: [
  //               {
  //                 $field: 'type',
  //                 $operator: '=',
  //                 $value: 'match'
  //               }
  //             ]
  //           }
  //         }
  //       }
  //     }
  //   ]
  // })

  // console.log('MATCH LAYOUT:', teamLayout)

  // const league = await client.set({
  //   $language: 'en',
  //   title: 'league one',
  //   children: [team]
  // })

  // const result = await client.get({
  //   $id: 'league1',
  //   id: true,
  //   $language: 'en',
  //   // theme: { $inherit: true },
  //   // ads: { $inherit: true },
  //   components: [
  //     {
  //       component: { $value: 'description' },
  //       title: {
  //         $field: 'title'
  //       },
  //       description: {
  //         $field: 'description'
  //       }
  //     },
  //     {
  //       component: { $value: 'gridLarge' },
  //       showall: { $value: true },
  //       children: {
  //         title: true,
  //         $list: {
  //           $range: [0, 100],
  //           $find: {
  //             $traverse: 'descendants',
  //             $filter: [
  //               {
  //                 $field: 'type',
  //                 $operator: '=',
  //                 $value: 'team'
  //               }
  //             ]
  //           }
  //         }
  //       }
  //     },
  //     {
  //       component: { $value: 'list' },
  //       children: {
  //         title: true,
  //         image: { icon: true, thumb: true },
  //         sport: { title: true, $inherit: { $item: 'sport' } },
  //         $list: {
  //           $sort: { $field: 'start', $order: 'asc' },
  //           $find: {
  //             $traverse: 'descendants',
  //             $filter: [
  //               {
  //                 $field: 'type',
  //                 $operator: '=',
  //                 $value: 'match'
  //               },
  //               {
  //                 $field: 'start',
  //                 $operator: '<',
  //                 $value: 'now'
  //               },
  //               {
  //                 $field: 'end',
  //                 $operator: '>',
  //                 $value: 'now'
  //               }
  //             ]
  //           }
  //         }
  //       }
  //     }
  //   ]
  // })

  // console.log(Date.now())
  // console.dir(result, { depth: 100 })

  // t.deepEqualIgnoreOrder(result, {
  //   id: 'league1',
  //   components: [
  //     {
  //       component: 'description',
  //       title: '🌊 mr flurpels 🌊',
  //       description: 'I like fancy 🌊'
  //     },
  //     {
  //       component: 'gridLarge',
  //       showall: true,
  //       children: [{ title: '🌊 TEAM 🌊' }]
  //     },
  //     {
  //       component: 'list',
  //       children: [{ sport: { title: 'flurp football' }, title: '🌊 MATCH 🌊' }]
  //     }
  //   ]
  // })
})
