import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'
import { deepCopy } from '@saulx/utils'

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
    languages: ['en', 'de', 'fr', 'it', 'nl'],
    rootType: {
      fields: {
        title: { type: 'text', search: { type: ['TEXT-LANGUAGE-SUG'] } }
      }
    },
    types: {
      folder: {
        prefix: 'fo',
        fields: {
          title: { type: 'text', search: { type: ['TEXT-LANGUAGE-SUG'] } }
        }
      },
      league: {
        prefix: 'le',
        fields: {
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      },
      team: {
        prefix: 'te',
        fields: {
          title: { type: 'text', search: { type: ['TEXT-LANGUAGE-SUG'] } },
          published: { type: 'boolean', search: { type: ['TAG'] } }
        }
      },
      video: {
        prefix: 'vi',
        fields: {
          title: { type: 'text', search: { type: ['TEXT-LANGUAGE-SUG'] } },
          published: { type: 'boolean', search: { type: ['TAG'] } }
        }
      },
      sport: {
        prefix: 'sp',
        fields: {
          title: { type: 'text', search: { type: ['TEXT-LANGUAGE-SUG'] } },
          published: { type: 'boolean', search: { type: ['TAG'] } }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text', search: { type: ['TEXT-LANGUAGE-SUG'] } },
          published: { type: 'boolean', search: { type: ['TAG'] } },
          homeTeam: { type: 'reference' },
          awayTeam: { type: 'reference' },
          startTime: {
            type: 'timestamp',
            search: { type: ['NUMERIC', 'SORTABLE'] }
          },
          endTime: {
            type: 'timestamp',
            search: { type: ['NUMERIC', 'SORTABLE'] }
          },
          date: {
            type: 'timestamp',
            search: { type: ['NUMERIC', 'SORTABLE'] }
          },
          fun: { type: 'set', items: { type: 'string' } },
          related: { type: 'references', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC'] } }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('subs layout', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  let now = Date.now()
  let viIdx = 0

  await client.set({
    $id: 'root',
    title: {
      en: 'root'
    }
  })

  await client.set({
    $id: 'te1',
    published: true,
    title: {
      en: 'team one',
      de: 'team ein'
    }
  })

  await client.set({
    $id: 'te2',
    published: true,
    title: {
      en: 'team two',
      de: 'team zwei'
    }
  })

  await client.set({
    $id: 'sp1',
    title: { en: 'sport one', de: 'sport ein' },
    published: true,
    children: ['te1', 'te2']
  })

  const highlights = await client.set({
    $id: 'fo1',
    title: {
      en: 'Highlights'
    },
    parents: ['sp1']
  })

  client
    .observe({
      $language: 'en',
      matches: {
        id: true,
        title: true,
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
                $field: 'published',
                $operator: '=',
                $value: true
              }
            ]
          }
        }
      }
    })
    .subscribe(r => {})
  client
    .observe({
      $language: 'de',
      matches: {
        id: true,
        title: true,
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
                $field: 'published',
                $operator: '=',
                $value: true
              }
            ]
          }
        }
      }
    })
    .subscribe(r => {})

  const past = []
  let pastPublishedIds = []
  for (let i = 0; i < 1000; i++) {
    const team = i % 2 === 0 ? 'te2' : 'te1'
    let published = true
    if (i % 3 == 0) {
      published = false
    }

    past.push(
      client.set({
        type: 'match',
        $id: 'map' + i,
        published,
        homeTeam: 'te1',
        awayTeam: 'te2',
        title: {
          en: 'past match ' + i,
          de: 'vorbei match ' + i,
          nl: 'verleden match ' + 1
        },
        name: 'past match',
        date: now - 1000 * 60 - i - 1,
        startTime: now - 1000 * 60 - i - 1,
        endTime: now - (1000 * 60 - i - 1),
        parents: [team, highlights],
        children: [{ $id: 'vi' + viIdx++, published: true }]
      })
    )

    if (published) {
      pastPublishedIds.push({ id: 'map' + i })
    }
  }

  await Promise.all(past)

  const upcoming = []
  const upcomingPublishedIds = []
  for (let i = 0; i < 1000; i++) {
    const team = i % 2 === 0 ? 'te2' : 'te1'
    let published = true
    if (i % 3 == 0) {
      published = false
    }

    upcoming.push(
      client.set({
        type: 'match',
        $id: 'maug' + i,
        published,
        name: 'past match',
        homeTeam: 'te1',
        awayTeam: 'te2',
        title: {
          en: 'gen upcoming match ' + i,
          de: 'gen kommend match ' + i,
          nl: 'gen aanstaande match ' + i
        },
        date: now + 1000 * 60 + i,
        startTime: now + 1000 * 60 + i,
        endTime: now + (1000 * 60 + i + 1),
        parents: [team, highlights],
        children: [{ $id: 'vi' + viIdx++, published: true }]
      })
    )

    if (published) {
      upcomingPublishedIds.push({ id: 'maug' + i })
    }
  }

  await Promise.all(upcomingPublishedIds)

  await wait(4000)
  now = Date.now()

  await Promise.all([
    client.set({
      type: 'match',
      $id: 'mau1',
      published: true,
      homeTeam: 'te1',
      awayTeam: 'te2',
      title: {
        en: 'upcoming match 1',
        de: 'kommend match 1',
        nl: 'aanstaande match 1'
      },
      name: 'upcoming match',
      date: now + 2000,
      parents: ['te1', highlights],
      startTime: now + 2000, // 2 sec from now
      endTime: now + 5000, // 5 sec from now
      children: [
        { $id: 'vi' + viIdx++, published: true },
        { $id: 'vi' + viIdx++, published: true }
      ]
    }),
    client.set({
      type: 'match',
      $id: 'mau2',
      homeTeam: 'te1',
      awayTeam: 'te2',
      title: {
        en: 'upcoming match 2',
        de: 'kommend match 2',
        nl: 'aanstaande match 2'
      },
      published: true,
      parents: ['te2'],
      name: 'upcoming match',
      date: now + 5000,
      startTime: now + 5000, // 5 sec from now
      endTime: now + 7000, // 7 sec from now
      children: [
        { $id: 'vi' + viIdx++, published: true },
        { $id: 'vi' + viIdx++, published: true }
      ]
    })
  ])

  let result
  client
    .observe({
      past: {
        id: true,
        $list: {
          $sort: {
            $field: 'date',
            $order: 'desc'
          },
          $limit: 10,
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $operator: '=',
                $value: 'match',
                $field: 'type'
              },
              {
                $operator: '=',
                $value: true,
                $field: 'published'
              },
              {
                $value: 'now',
                $field: 'endTime',
                $operator: '<'
              }
            ]
          }
        }
      },
      live: {
        id: true,
        $list: {
          $sort: {
            $field: 'date',
            $order: 'asc'
          },
          $limit: 10,
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $operator: '=',
                $value: 'match',
                $field: 'type'
              },
              {
                $operator: '=',
                $value: true,
                $field: 'published'
              },
              {
                $value: 'now',
                $field: 'startTime',
                $operator: '<'
              },
              {
                $value: 'now',
                $field: 'endTime',
                $operator: '>'
              }
            ]
          }
        }
      },
      upcoming: {
        id: true,
        $list: {
          $sort: {
            $field: 'date',
            $order: 'asc'
          },
          $limit: 10,
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $operator: '=',
                $value: true,
                $field: 'published'
              },
              {
                $operator: '=',
                $value: 'match',
                $field: 'type'
              },
              {
                $value: 'now',
                $field: 'startTime',
                $operator: '>'
              }
            ]
          }
        }
      }
    })
    .subscribe((r, v, d) => {
      result = deepCopy(r)
    })

  let otherResult1
  client
    .observe({
      $id: 'mau1',
      $language: 'en',
      components: [
        {
          component: {
            $value: 'Table'
          },
          title: {
            $value: 'Live'
          },
          children: {
            teams: [
              {
                id: true,
                $id: {
                  $field: 'homeTeam'
                },
                title: true
              },
              {
                id: true,
                $id: {
                  $field: 'awayTeam'
                },
                title: true
              }
            ],
            type: true,
            title: true,
            id: true,
            $list: {
              $limit: 30,
              $find: {
                $filter: [
                  {
                    $field: 'type',
                    $operator: '=',
                    $value: 'sport'
                  },
                  {
                    $field: 'published',
                    $operator: '=',
                    $value: true
                  }
                ],
                $find: {
                  $traverse: 'descendants',
                  $filter: [
                    {
                      $field: 'type',
                      $operator: '=',
                      $value: 'match'
                    },
                    {
                      $field: 'published',
                      $operator: '=',
                      $value: true
                    },
                    {
                      $operator: '<',
                      $value: 'now',
                      $field: 'startTime'
                    },
                    {
                      $operator: '>',
                      $value: 'now',
                      $field: 'endTime'
                    }
                  ]
                },
                $traverse: 'ancestors'
              },
              $sort: {
                $order: 'desc',
                $field: 'date'
              }
            }
          }
        },
        {
          component: {
            $value: 'GridLarge'
          },
          title: {
            $value: 'Team Videos'
          },
          children: {
            type: true,
            title: true,
            $list: {
              $limit: 10,
              $find: {
                $filter: [
                  {
                    $field: 'type',
                    $operator: '=',
                    $value: 'team'
                  },
                  {
                    $field: 'published',
                    $operator: '=',
                    $value: true
                  }
                ],
                $find: {
                  $traverse: 'descendants',
                  $filter: [
                    {
                      $field: 'type',
                      $operator: '=',
                      $value: 'video'
                    },
                    {
                      $field: 'published',
                      $operator: '=',
                      $value: true
                    }
                  ]
                },
                $traverse: 'ancestors'
              },
              $sort: {
                $order: 'desc',
                $field: 'date'
              }
            },
            id: true
          }
        }
      ]
    })
    .subscribe(r => {
      otherResult1 = deepCopy(r)
    })

  let otherResult2
  client
    .observe({
      $id: 'mau2',
      $language: 'en',
      components: [
        {
          component: {
            $value: 'Table'
          },
          title: {
            $value: 'Live'
          },
          children: {
            teams: [
              {
                id: true,
                $id: {
                  $field: 'homeTeam'
                },
                title: true
              },
              {
                id: true,
                $id: {
                  $field: 'awayTeam'
                },
                title: true
              }
            ],
            type: true,
            title: true,
            id: true,
            $list: {
              $limit: 30,
              $find: {
                $filter: [
                  {
                    $field: 'type',
                    $operator: '=',
                    $value: 'sport'
                  },
                  {
                    $field: 'published',
                    $operator: '=',
                    $value: true
                  }
                ],
                $find: {
                  $traverse: 'descendants',
                  $filter: [
                    {
                      $field: 'type',
                      $operator: '=',
                      $value: 'match'
                    },
                    {
                      $field: 'published',
                      $operator: '=',
                      $value: true
                    },
                    {
                      $operator: '<',
                      $value: 'now',
                      $field: 'startTime'
                    },
                    {
                      $operator: '>',
                      $value: 'now',
                      $field: 'endTime'
                    }
                  ]
                },
                $traverse: 'ancestors'
              },
              $sort: {
                $order: 'desc',
                $field: 'date'
              }
            }
          }
        },
        {
          component: {
            $value: 'GridLarge'
          },
          title: {
            $value: 'Team Videos'
          },
          children: {
            type: true,
            title: true,
            $list: {
              $limit: 10,
              $find: {
                $filter: [
                  {
                    $field: 'type',
                    $operator: '=',
                    $value: 'team'
                  },
                  {
                    $field: 'published',
                    $operator: '=',
                    $value: true
                  }
                ],
                $find: {
                  $traverse: 'descendants',
                  $filter: [
                    {
                      $field: 'type',
                      $operator: '=',
                      $value: 'video'
                    },
                    {
                      $field: 'published',
                      $operator: '=',
                      $value: true
                    }
                  ]
                },
                $traverse: 'ancestors'
              },
              $sort: {
                $order: 'desc',
                $field: 'date'
              }
            },
            id: true
          }
        }
      ]
    })
    .subscribe(r => {
      otherResult2 = deepCopy(r)
    })

  let otherResult3
  client
    .observe({
      $id: 'sp1',
      id: true,
      $language: 'nl',
      type: true,
      ancestors: true,
      general: {
        $id: 'root',
        title: {
          $field: 'title'
        }
      },
      meta: {
        title: {
          $field: 'title'
        }
      },
      components: [
        {
          component: {
            $value: 'Highlights'
          },
          title: {
            $value: 'Highlights'
          },
          children: {
            title: true,
            $list: {
              $limit: 100,
              $find: {
                $filter: [
                  {
                    $operator: '=',
                    $value: 'folder',
                    $field: 'type'
                  },
                  {
                    $operator: '=',
                    $value: 'Highlights',
                    $field: 'title'
                  }
                ],
                $find: {
                  $traverse: 'descendants',
                  $filter: [
                    {
                      $operator: '=',
                      $value: true,
                      $field: 'published'
                    }
                  ]
                },
                $traverse: 'descendants'
              },
              $sort: {
                $order: 'desc',
                $field: 'date'
              }
            },
            teams: [
              {
                id: true,
                $id: {
                  $field: 'homeTeam'
                },
                title: true
              },
              {
                id: true,
                $id: {
                  $field: 'awayTeam'
                },
                title: true
              }
            ],
            date: true,
            type: true,
            id: true
          }
        },
        {
          component: {
            $value: 'Table'
          },
          title: {
            $value: 'Live Now'
          },
          children: {
            teams: [
              {
                id: true,
                $id: {
                  $field: 'homeTeam'
                },
                title: true
              },
              {
                id: true,
                $id: {
                  $field: 'awayTeam'
                },
                title: true
              }
            ],
            type: true,
            title: true,
            date: true,
            startTime: true,
            id: true,
            $list: {
              $limit: 15,
              $find: {
                $traverse: 'descendants',
                $filter: [
                  {
                    $value: 'match',
                    $field: 'type',
                    $operator: '='
                  },
                  {
                    $value: true,
                    $field: 'published',
                    $operator: '='
                  },
                  {
                    $field: 'startTime',
                    $operator: '<',
                    $value: 'now'
                  },
                  {
                    $field: 'endTime',
                    $operator: '>',
                    $value: 'now'
                  }
                ]
              },
              $sort: {
                $order: 'desc',
                $field: 'date'
              }
            }
          }
        }
      ]
    })
    .subscribe(r => {
      otherResult3 = deepCopy(r)
    })

  await wait(500)
  t.deepEqualIgnoreOrder(
    result,
    {
      upcoming: [{ id: 'mau1' }, { id: 'mau2' }].concat(
        upcomingPublishedIds.slice(0, 8)
      ),
      past: pastPublishedIds.slice(0, 10),
      live: []
    },
    'first assertion'
  )
  t.deepEqualIgnoreOrder(otherResult1.components[0].children, [])
  t.is(otherResult1.components[1].children.length, 10)
  t.deepEqualIgnoreOrder(otherResult2.components[0].children, [])
  t.is(otherResult2.components[1].children.length, 10)
  const pick = ({ id, type, ancestors, general, meta }) => ({
    id,
    type,
    ancestors,
    general,
    meta
  })
  t.deepEqualIgnoreOrder(pick(otherResult3), {
    id: 'sp1',
    type: 'sport',
    ancestors: ['root'],
    general: {
      title: 'root'
    },
    meta: {
      title: 'sport one'
    }
  })
  t.is(otherResult3.components[0].children.length, 100)
  t.is(otherResult3.components[1].children.length, 0)

  await wait(3000)

  const expect = {
    upcoming: [{ id: 'mau2' }].concat(upcomingPublishedIds.slice(0, 9)),
    past: pastPublishedIds.slice(0, 10),
    live: [{ id: 'mau1' }]
  }

  t.deepEqualIgnoreOrder(result, expect, 'upcoming 2')
  t.deepEqualIgnoreOrder(
    otherResult1.components[0].children,
    [
      {
        id: 'mau1',
        type: 'match',
        teams: [
          { id: 'te1', title: 'team one' },
          { id: 'te2', title: 'team two' }
        ],
        title: 'upcoming match 1'
      }
    ],
    'upcoming 3'
  )
  t.deepEqualIgnoreOrder(otherResult1.components[1].children.length, 10)
  t.deepEqualIgnoreOrder(
    otherResult2.components[0].children,
    [
      {
        id: 'mau1',
        type: 'match',
        teams: [
          { id: 'te1', title: 'team one' },
          { id: 'te2', title: 'team two' }
        ],
        title: 'upcoming match 1'
      }
    ],
    'upcoming 4'
  )
  t.is(otherResult2.components[1].children.length, 10)
  t.deepEqualIgnoreOrder(
    pick(otherResult3),
    {
      id: 'sp1',
      type: 'sport',
      ancestors: ['root'],
      general: {
        title: 'root'
      },
      meta: {
        title: 'sport one'
      }
    },
    'upcoming 5'
  )
  t.is(otherResult3.components[0].children.length, 100)
  t.is(otherResult3.components[1].children.length, 1)
  t.is(otherResult3.components[1].children[0].id, 'mau1')

  await wait(3000)

  t.deepEqualIgnoreOrder(
    result,
    {
      upcoming: upcomingPublishedIds.slice(0, 10),
      past: [{ id: 'mau1' }].concat(pastPublishedIds.slice(0, 9)),
      live: [{ id: 'mau2' }]
    },
    'upcoming 6'
  )
  t.deepEqualIgnoreOrder(
    otherResult1.components[0].children,
    [
      {
        id: 'mau2',
        type: 'match',
        teams: [
          { id: 'te1', title: 'team one' },
          { id: 'te2', title: 'team two' }
        ],
        title: 'upcoming match 2'
      }
    ],
    'upcoming 7'
  )
  t.is(otherResult1.components[1].children.length, 10)
  t.deepEqualIgnoreOrder(
    otherResult2.components[0].children,
    [
      {
        id: 'mau2',
        type: 'match',
        teams: [
          { id: 'te1', title: 'team one' },
          { id: 'te2', title: 'team two' }
        ],
        title: 'upcoming match 2'
      }
    ],
    'upcoming 8'
  )
  t.deepEqualIgnoreOrder(otherResult2.components[1].children.length, 10)
  t.deepEqualIgnoreOrder(
    pick(otherResult3),
    {
      id: 'sp1',
      type: 'sport',
      ancestors: ['root'],
      general: {
        title: 'root'
      },
      meta: {
        title: 'sport one'
      }
    },
    'upcoming 9'
  )
  t.is(otherResult3.components[0].children.length, 100)
  t.is(otherResult3.components[1].children.length, 1)
  t.is(otherResult3.components[1].children[0].id, 'mau2')

  await wait(2000)

  t.deepEqualIgnoreOrder(
    result,
    {
      upcoming: upcomingPublishedIds.slice(0, 10),
      past: [{ id: 'mau1' }, { id: 'mau2' }].concat(
        pastPublishedIds.slice(0, 8)
      ),
      live: []
    },
    'upcoming 10'
  )
  t.deepEqualIgnoreOrder(otherResult1.components[0].children, [])
  t.is(otherResult1.components[1].children.length, 10)
  t.deepEqualIgnoreOrder(otherResult2.components[0].children, [])
  t.is(otherResult2.components[1].children.length, 10)
  t.deepEqualIgnoreOrder(
    pick(otherResult3),
    {
      id: 'sp1',
      type: 'sport',
      ancestors: ['root'],
      general: {
        title: 'root'
      },
      meta: {
        title: 'sport one'
      }
    },
    'upcoming 11'
  )
  t.is(otherResult3.components[0].children.length, 100)
  t.is(otherResult3.components[1].children.length, 0)

  await client.delete('root')
  await client.destroy()

  await wait(3e3)

  t.true(true)
})

test.serial('subs upcoming, live and past', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  const now = Date.now()
  let result

  await client.set({
    type: 'match',
    $id: 'ma1',
    name: 'upcoming match',
    startTime: now + 2000, // 2 sec from now
    endTime: now + 5000 // 5 sec from now
  })

  client
    .observe({
      past: {
        id: true,
        $list: {
          $limit: 10,
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $operator: '=',
                $value: 'match',
                $field: 'type'
              },
              {
                $value: 'now',
                $field: 'endTime',
                $operator: '<'
              }
            ]
          }
        }
      },
      live: {
        id: true,
        $list: {
          $limit: 10,
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $operator: '=',
                $value: 'match',
                $field: 'type'
              },
              {
                $value: 'now',
                $field: 'startTime',
                $operator: '<'
              },
              {
                $value: 'now',
                $field: 'endTime',
                $operator: '>'
              }
            ]
          }
        }
      },
      upcoming: {
        id: true,
        $list: {
          $limit: 10,
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $operator: '=',
                $value: 'match',
                $field: 'type'
              },
              {
                $value: 'now',
                $field: 'startTime',
                $operator: '>'
              }
            ]
          }
        }
      }
    })
    .subscribe(r => {
      result = deepCopy(r)
    })

  await wait(500)

  t.deepEqualIgnoreOrder(result, {
    upcoming: [{ id: 'ma1' }],
    past: [],
    live: []
  })

  await wait(3000)

  t.deepEqualIgnoreOrder(result, {
    upcoming: [],
    past: [],
    live: [{ id: 'ma1' }]
  })

  await wait(3000)

  t.deepEqualIgnoreOrder(result, {
    upcoming: [],
    past: [{ id: 'ma1' }],
    live: []
  })

  await client.delete('root')
  await client.destroy()

  await wait(3e3)
  await client.destroy()

  t.true(true)
})

test.serial('find - already started', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  const match1 = await client.set({
    type: 'match',
    name: 'started 5m ago',
    startTime: Date.now() - 5 * 60 * 1000, // 5 minutes ago
    endTime: Date.now() + 60 * 60 * 1000 // ends in 1 hour
  })

  await client.set({
    type: 'match',
    name: 'started 2m ago',
    startTime: Date.now() - 2 * 60 * 1000, // 2 minutes ago
    endTime: Date.now() + 60 * 60 * 1000 // ends in 1 hour
  })

  await client.set({
    type: 'match',
    name: 'started 2h ago',
    startTime: Date.now() - 2 * 60 * 60 * 1000, // 2 hours ago
    endTime: Date.now() - 60 * 60 * 1000 // ended 1 hour ago
  })

  const nextRefresh = Date.now() + 1 * 60 * 60 * 1000
  await client.set({
    $id: 'maFuture',
    type: 'match',
    name: 'starts in 1h',
    startTime: nextRefresh, // starts in 1 hour
    endTime: Date.now() + 2 * 60 * 60 * 1000 // ends in 2 hours
  })

  await client.set({
    $id: 'maLaterFuture',
    type: 'match',
    name: 'starts in 2h',
    startTime: Date.now() + 2 * 60 * 60 * 1000, // starts in 1 hour
    endTime: Date.now() + 3 * 60 * 60 * 1000 // ends in 2 hours
  })

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $includeMeta: true,
        $id: 'root',
        items: {
          name: true,
          value: true,
          $list: {
            $sort: { $field: 'startTime', $order: 'desc' },
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'startTime',
                  $operator: '<',
                  $value: 'now'
                }
              ]
            }
          }
        }
      })
    ).$meta.___refreshAt,
    nextRefresh
  )
  await client.delete('root')
  await client.destroy()

  t.true(true)
})

test.serial('find - already started subscription', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    type: 'match',
    name: 'started 5m ago',
    startTime: Date.now() - 5 * 60 * 1000, // 5 minutes ago
    endTime: Date.now() + 60 * 60 * 1000 // ends in 1 hour
  })

  await client.set({
    type: 'match',
    name: 'started 2m ago',
    startTime: Date.now() - 2 * 60 * 1000, // 2 minutes ago
    endTime: Date.now() + 60 * 60 * 1000 // ends in 1 hour
  })

  await client.set({
    type: 'match',
    name: 'started 2h ago',
    startTime: Date.now() - 2 * 60 * 60 * 1000, // 2 hours ago
    endTime: Date.now() - 60 * 60 * 1000 // ended 1 hour ago
  })

  const nextRefresh = Date.now() + 5 * 1000
  const nextNextRefresh = Date.now() + 7 * 1000

  // add another <============== THIS BREAKS IT
  client
    .observe({
      $id: 'rando',
      items: {
        name: true,
        value: true,
        $list: {
          $find: {
            $traverse: 'children',
            $filter: [
              {
                $field: 'endTime',
                $operator: '<',
                $value: 'now'
              }
            ]
          }
        }
      }
    })
    .subscribe(() => {})
  // =======================

  await client.set({
    $id: 'maFuture',
    type: 'match',
    name: 'starts in 5s',
    startTime: nextRefresh,
    endTime: Date.now() + 2 * 60 * 60 * 1000 // ends in 2 hours
  })

  await client.set({
    $id: 'maLaterFuture',
    type: 'match',
    name: 'starts in 7s',
    startTime: nextNextRefresh,
    endTime: Date.now() + 3 * 60 * 60 * 1000 // ends in 2 hours
  })

  const observable = client.observe({
    $includeMeta: true,
    $id: 'root',
    items: {
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'startTime', $order: 'asc' },
        $find: {
          $traverse: 'children',
          $filter: [
            {
              $field: 'startTime',
              $operator: '<',
              $value: 'now'
            }
          ]
        }
      }
    }
  })

  await wait(100)

  let o1counter = 0
  const sub = observable.subscribe(d => {
    if (o1counter === 0) {
      // gets start event
      t.true(d.items.length === 3)
    } else if (o1counter === 1) {
      // gets update event
      t.true(d.items.length === 4)
      t.true(d.items.map(i => i.name).includes('starts in 5s'))
    } else if (o1counter === 2) {
      t.true(d.items.length === 5)
      t.true(d.items.map(i => i.name).includes('starts in 7s'))
    } else {
      // doesn't get any more events
      t.fail()
    }
    o1counter++
  })

  await wait(10 * 1000)

  sub.unsubscribe()

  await wait(100)

  await client.delete('root')
  await client.destroy()

  await wait(1000)

  t.true(true)
})

test.serial('find - starting soon', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  const match1 = await client.set({
    type: 'match',
    name: 'started 5m ago',
    startTime: Date.now() - 5 * 60 * 1000, // 5 minutes ago
    endTime: Date.now() + 60 * 60 * 1000 // ends in 1 hour
  })

  await client.set({
    type: 'match',
    name: 'started 2m ago',
    startTime: Date.now() - 2 * 60 * 1000, // 2 minutes ago
    endTime: Date.now() + 60 * 60 * 1000 // ends in 1 hour
  })

  await client.set({
    type: 'match',
    name: 'started 2h ago',
    startTime: Date.now() - 2 * 60 * 60 * 1000, // 2 hours ago
    endTime: Date.now() - 60 * 60 * 1000 // ended 1 hour ago
  })

  const nextRefresh = Date.now() + 1 * 60 * 60 * 1000
  await client.set({
    $id: 'maFuture',
    type: 'match',
    name: 'starts in 1h',
    startTime: nextRefresh, // starts in 1 hour
    endTime: Date.now() + 2 * 60 * 60 * 1000 // ends in 2 hours
  })

  await client.set({
    $id: 'maLaterFuture',
    type: 'match',
    name: 'starts in 2h',
    startTime: Date.now() + 2 * 60 * 60 * 1000, // starts in 2 hour
    endTime: Date.now() + 3 * 60 * 60 * 1000 // ends in 3 hours
  })

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $includeMeta: true,
        $id: 'root',
        items: {
          name: true,
          value: true,
          $list: {
            $sort: { $field: 'startTime', $order: 'asc' },
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'startTime',
                  $operator: '>',
                  $value: 'now+1h'
                },
                {
                  $field: 'startTime',
                  $operator: '<',
                  $value: 'now+3h'
                }
              ]
            }
          }
        }
      })
    ).items.map(i => i.name),
    ['starts in 2h']
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $includeMeta: true,
        $id: 'root',
        items: {
          name: true,
          value: true,
          $list: {
            $sort: { $field: 'startTime', $order: 'asc' },
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'startTime',
                  $operator: '..',
                  $value: ['now+1h', 'now+3h']
                }
              ]
            }
          }
        }
      })
    ).items.map(i => i.name),
    ['starts in 2h']
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $includeMeta: true,
        $id: 'root',
        items: {
          name: true,
          value: true,
          $list: {
            $sort: { $field: 'startTime', $order: 'asc' },
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'startTime',
                  $operator: '>',
                  $value: 'now-6m'
                },
                {
                  $field: 'startTime',
                  $operator: '<',
                  $value: 'now'
                }
              ]
            }
          }
        }
      })
    ).items.map(i => i.name),
    ['started 5m ago', 'started 2m ago']
  )

  await client.destroy()

  t.true(true)
})
