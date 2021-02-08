import test from 'ava'
import './assertions'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { dumpDb, idExists } from './assertions'
import getPort from 'get-port'
import { wait } from '../src/util'

const DEFAULT_HIERARCHY = '___selva_hierarchy'

let srv
let port: number

test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })

  const client = connect(
    {
      port
    },
    { loglevel: 'info' }
  )

  // await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'nl', 'de'],
    rootType: {
      fields: { value: { type: 'number' } }
    },
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: {
            type: 'text'
          },
          obj: {
            type: 'object',
            properties: {
              hello: { type: 'string' },
              hallo: { type: 'string' }
            }
          },
          nestedObj: {
            type: 'object',
            properties: {
              a: {
                type: 'object',
                properties: {
                  value: { type: 'string' }
                }
              },
              b: {
                type: 'object',
                properties: {
                  value: { type: 'string' }
                }
              }
            }
          },
          settySet: {
            type: 'set',
            items: {
              type: 'string'
            }
          },
          reffyRefs: {
            type: 'references'
          },
          reffyRef: {
            type: 'reference'
          },
          createdAt: { type: 'timestamp' }
        }
      },
      league: {
        prefix: 'cu',
        fields: {
          title: {
            type: 'text'
          },
          createdAt: { type: 'number' }
        }
      },
      person: {
        prefix: 'pe',
        fields: {
          title: {
            type: 'text'
          },
          createdAt: { type: 'timestamp' },
          updatedAt: { type: 'timestamp' }
        }
      },
      someTestThing: {
        prefix: 'vi',
        fields: {
          title: {
            type: 'text'
          },
          value: {
            type: 'number'
          }
        }
      },
      otherTestThing: {
        prefix: 'ar',
        fields: {
          title: {
            type: 'text'
          },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('root', async t => {
  const client = connect(
    {
      port
    },
    { loglevel: 'info' }
  )

  const match = await client.set({
    type: 'match'
  })

  const root = await client.set({
    $id: 'root',
    value: 9001
  })

  t.deepEqual(root, 'root')
  t.deepEqual(await client.redis.selva_object_get('root', 'value'), '9001')
  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'),
    [match]
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('root.children $delete: []', async t => {
  const client = connect(
    {
      port
    },
    { loglevel: 'info' }
  )

  const match = await client.set({
    type: 'match'
  })

  const root = await client.set({
    $id: 'root',
    children: [match]
  })

  t.deepEqual(root, 'root')
  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'),
    [match]
  )

  await client.set({
    $id: 'root',
    children: { $delete: [] }
  })

  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'),
    [match]
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('basic', async t => {
  const client = connect(
    {
      port
    },
    {
      loglevel: 'info'
    }
  )

  const match = await client.set({
    type: 'match'
  })

  const league = await client.set({
    type: 'league'
  })

  const person = await client.set({
    type: 'person',
    parents: [match],
    title: { en: 'flurpy man' }
  })

  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, match),
    [person],
    'match has correct children'
  )

  t.deepEqual(
    (
      await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root')
    ).sort(),
    [league, match].sort(),
    'root has correct children'
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, league),
    [],
    'league has no children'
  )

  t.is(
    await client.redis.selva_object_get(person, 'title.en'),
    'flurpy man',
    'Title of person is correctly set'
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      match
    ),
    ['root']
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      league
    ),
    ['root']
  )

  // // move person from match to league
  await client.set({
    $id: person,
    parents: [league]
  })

  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, league),
    [person],
    'league has person after move'
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, match),
    [],
    'match has no children after move'
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      person
    ),
    ['root', league]
  )

  // // add extra parent using $add
  await client.set({
    $id: person,
    parents: {
      $add: match
    }
  })

  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, match),
    [person],
    'match has children after $add'
  )

  t.deepEqual(
    (
      await client.redis.selva_hierarchy_parents(DEFAULT_HIERARCHY, person)
    ).sort(),
    [league, match].sort(),
    'person has correct parents after $add'
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      person
    ),
    ['root', league, match]
  )

  // remove league from person
  await client.set({
    $id: person,
    parents: {
      $delete: league
    }
  })

  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, league),
    [],
    'league has no children after $delete'
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_parents(DEFAULT_HIERARCHY, person),
    [match],
    'person has correct parents after $delete'
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      person
    ),
    ['root', match]
  )

  // add parent again
  await client.set({
    $id: person,
    parents: {
      $add: league
    }
  })

  // double add
  await client.set({
    $id: person,
    parents: {
      $add: league
    }
  })

  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, match),
    [person],
    'match has children after 2nd $add'
  )

  t.deepEqual(
    (
      await client.redis.selva_hierarchy_parents(DEFAULT_HIERARCHY, person)
    ).sort(),
    [league, match].sort(),
    'person has correct parents after 2nd $add'
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      person
    ),
    ['root', match, league]
  )

  // reset children
  await client.set({
    $id: match,
    children: []
  })

  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, match),
    [],
    'match has no children after reset'
  )

  // add no children
  await client.set({
    $id: match,
    children: { $add: [] }
  })

  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, match),
    [],
    'match has no children after $add: []'
  )

  // set null children
  await t.throwsAsync(
    client.set({
      $id: match,
      children: null
    })
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, match),
    [],
    'match has no children after children: null'
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_parents(DEFAULT_HIERARCHY, person),
    [league],
    'person has correct parents after reset of children of match'
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      person
    ),
    ['root', league]
  )

  // add person to match using children
  await client.set({
    $id: match,
    children: [person]
  })

  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, match),
    [person],
    'match has children after adding person to match using children'
  )

  t.deepEqual(
    (
      await client.redis.selva_hierarchy_parents(DEFAULT_HIERARCHY, person)
    ).sort(),
    [league, match].sort(),
    'person has correct parents after adding person to match using children'
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      person
    ),
    ['root', league, match]
  )

  // add match to league using $add
  await client.set({
    $id: league,
    children: { $add: match }
  })

  t.deepEqual(
    (
      await client.redis.selva_hierarchy_parents(DEFAULT_HIERARCHY, match)
    ).sort(),
    ['root', league].sort(),
    'match has correct parents after adding match as a child to league'
  )

  t.deepEqual(
    (
      await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, league)
    ).sort(),
    [match, person].sort(),
    'league has correct children after setting ancestors'
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      person
    ),
    ['root', league, match]
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      match
    ),
    ['root', league]
  )

  // delete match from league
  await client.set({
    $id: league,
    children: { $delete: match }
  })

  t.deepEqual(
    (
      await client.redis.selva_hierarchy_parents(DEFAULT_HIERARCHY, person)
    ).sort(),
    [league, match].sort(),
    'person has correct parents after removing match from league'
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      person
    ),
    ['root', league, match]
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_parents(DEFAULT_HIERARCHY, match),
    ['root'].sort(),
    'match has correct parents after removing match from league'
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      match
    ),
    ['root'],
    'match has correct ancestors after removing match from league'
  )

  // delete person
  await client.delete(person)
  t.false(
    await idExists(client, person),
    'person is removed from db after delete'
  )

  // delete league
  await client.delete(league)
  t.false(
    await idExists(client, league),
    'league is removed from db after delete'
  )

  // delete root
  await client.delete('root')
  await client.destroy()
})

test.serial('deep hierarchy manipulation', async t => {
  const client = connect({
    port
  })

  await client.set({
    $id: 'cuX',
    children: ['cuA']
  })

  await client.set({
    $id: 'cuA',
    children: ['cuB', 'cuC', 'cuD']
  })

  await client.set({
    $id: 'cuE',
    parents: ['cuD']
  })

  await client.set({
    $id: 'cuD',
    parents: { $add: 'root' }
  })

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      'cuB'
    ),
    ['root', 'cuX', 'cuA']
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      'cuC'
    ),
    ['root', 'cuX', 'cuA']
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      'cuD'
    ),
    ['root', 'cuX', 'cuA']
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      'cuE'
    ),
    ['root', 'cuX', 'cuA', 'cuD']
  )

  console.log(
    '###',
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      'cuD'
    )
  )
  await client.set({
    $id: 'cuD',
    parents: { $delete: 'cuA' }
  })

  console.log(
    '???',
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      'cuD'
    )
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      'cuD'
    ),
    ['root']
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '___selva_hierarchy',
      'bfs',
      'ancestors',
      'cuE'
    ),
    ['root', 'cuD']
  )

  await client.destroy()
})

test.serial('array, json and set', async t => {
  const client = connect({
    port
  })

  await client.updateSchema({
    types: {
      flurp: {
        prefix: 'FU',
        fields: {
          flurpy: {
            type: 'json',
            properties: {
              hello: {
                // need to check if you are already
                // in json or array and then you need to  strip default options etc
                type: 'array',
                items: {
                  type: 'string'
                }
              }
            }
          },
          flap: {
            type: 'array',
            items: {
              type: 'object',
              properties: {
                gurk: { type: 'string' }
                //flap: { type: 'digest' }
              }
            }
          }
        }
      }
    }
  })
  const id = await client.set({
    type: 'flurp',
    flap: [
      {
        gurk: 'hello'
        //flap: 'smurpy'
      }
    ]
  })
  const r = JSON.parse(await client.redis.selva_object_get(id, 'flap'))
  t.deepEqual(r, [
    {
      gurk: 'hello'
      //flap: '6734082360af7f0c5aef4123f43abc44c4fbf19e8b251a316d7b9da95fde448e'
    }
  ])

  await client.destroy()
})

test.serial('set empty object', async t => {
  const client = connect({
    port
  })

  await client.updateSchema({
    types: {
      hmmhmm: {
        prefix: 'hm',
        fields: {
          flurpy: {
            type: 'object',
            properties: {
              hello: {
                type: 'string'
              }
            }
          }
        }
      }
    }
  })
  const id = await client.set({
    type: 'hmmhmm',
    flurpy: {}
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: id,
      flurpy: true
    }),
    {}
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: id,
      flurpy: {
        hello: true
      }
    }),
    {}
  )

  await client.set({
    $id: id,
    flurpy: { hello: 'yes' }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: id,
      flurpy: true
    }),
    {
      flurpy: {
        hello: 'yes'
      }
    }
  )

  await client.destroy()
})

test.serial('$increment, $default', async t => {
  const client = connect({
    port
  })
  await client.set({
    $id: 'viDingDong',
    value: {
      $default: 100,
      $increment: 10
    }
  })

  t.is(
    await client.redis.selva_object_get('viDingDong', 'value'),
    '100',
    'uses default if value does not exist'
  )

  await client.set({
    $id: 'viDingDong',
    value: {
      $default: 100,
      $increment: 10
    }
  })

  t.is(
    await client.redis.selva_object_get('viDingDong', 'value'),
    '110',
    'increment if value exists'
  )

  await client.set({
    $id: 'viDingDong',
    title: {
      en: {
        $default: 'title'
      }
    }
  })

  t.is(
    await client.redis.selva_object_get('viDingDong', 'title.en'),
    'title',
    'set default'
  )

  await client.set({
    $id: 'viDingDong',
    title: {
      en: {
        $default: 'flurp'
      }
    }
  })

  t.is(
    await client.redis.selva_object_get('viDingDong', 'title.en'),
    'title',
    'does not overwrite if value exists'
  )

  await client.delete('root')

  await client.destroy()
})

test.serial('$merge = false', async t => {
  const client = connect({
    port
  })

  await client.set({
    $id: 'arPower',
    title: {
      en: 'flap',
      de: 'flurpels'
    },
    image: {
      thumb: 'x'
    }
  })

  t.is(await client.redis.selva_object_get('arPower', 'title.en'), 'flap')
  t.is(await client.redis.selva_object_get('arPower', 'title.de'), 'flurpels')

  await client.set({
    $id: 'arPower',
    $merge: false,
    title: {
      de: 'deutschland'
    }
  })

  t.is(await client.redis.selva_object_get('arPower', 'id'), 'arPower')
  t.is(await client.redis.selva_object_get('arPower', 'title.en'), null)
  t.is(
    await client.redis.selva_object_get('arPower', 'title.de'),
    'deutschland'
  )

  await client.set({
    $id: 'arPower',
    title: {
      $merge: false,
      nl: 'nl'
    }
  })

  t.is(await client.redis.selva_object_get('arPower', 'title.nl'), 'nl')
  t.is(await client.redis.selva_object_get('arPower', 'title.de'), null)

  await client.set({
    $id: 'arPower',
    image: {
      $merge: false,
      poster: 'x'
    }
  })

  t.is(await client.redis.selva_object_get('arPower', 'image.thumb'), null)

  await client.delete('root')
})

test.serial('automatic child creation', async t => {
  const client = connect(
    {
      port
    },
    { loglevel: 'info' }
  )

  const parent = await client.set({
    $id: 'viParent',
    title: {
      nl: 'nl'
    },
    children: [
      {
        type: 'match',
        title: {
          nl: 'child1'
        }
      },
      {
        type: 'match',
        title: {
          nl: 'child2'
        }
      },
      {
        type: 'match',
        title: {
          nl: 'child3'
        }
      }
    ]
  })

  const children = await client.redis.selva_hierarchy_children(
    DEFAULT_HIERARCHY,
    parent
  )
  t.is(children.length, 3, 'Should have 3 children created')

  const titles = (
    await Promise.all(
      children.map(child => {
        return client.redis.selva_object_get(child, 'title.nl')
      })
    )
  ).sort()
  for (let i = 0; i < titles.length; i++) {
    t.is(titles[i], 'child' + (i + 1), `Child ${i} title should match`)
  }

  await client.set({
    $id: parent,
    children: {
      $add: [
        {
          $id: 'maTestWithId',
          title: {
            nl: 'yes with id'
          }
        }
      ]
    }
  })

  await client.set({
    $id: parent,
    type: 'match',
    children: {
      $add: [
        {
          type: 'match',
          $alias: 'maTestWithAlias',
          title: {
            nl: 'yes with alias'
          }
        }
      ]
    }
  })

  const newChildren = await client.redis.selva_hierarchy_children(
    DEFAULT_HIERARCHY,
    parent
  )
  t.is(newChildren.length, 5, 'Should have 5 children created')

  await client.destroy()
})

test.serial('createdAt set if defined as timestamp', async t => {
  const client = connect({
    port
  })

  const before = Date.now()
  const match = await client.set({
    $language: 'en',
    type: 'match',
    title: 'yesh'
  })
  const after = Date.now()

  const result = await client.get({
    $language: 'en',
    $id: match,
    id: true,
    title: true,
    createdAt: true,
    updatedAt: true
  })

  const createdAt = result.createdAt
  delete result.createdAt

  t.deepEqual(result, {
    id: match,
    title: 'yesh'
  })

  t.true(
    typeof createdAt === 'number' && createdAt <= after && createdAt >= before
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('createdAt+updatedAt set if defined as timestamp', async t => {
  const client = connect({
    port
  })

  const before = Date.now()
  const person = await client.set({
    $language: 'en',
    type: 'person',
    title: 'yesh'
  })
  const after = Date.now()

  const result = await client.get({
    $language: 'en',
    $id: person,
    id: true,
    title: true,
    createdAt: true,
    updatedAt: true
  })

  const createdAt = result.createdAt
  const updatedAt = result.updatedAt
  delete result.createdAt
  delete result.updatedAt

  t.deepEqual(result, {
    id: person,
    title: 'yesh'
  })

  t.true(
    typeof createdAt === 'number' && createdAt <= after && createdAt >= before
  )

  t.deepEqual(createdAt, updatedAt)

  await client.delete('root')
  await client.destroy()
})

test.serial('createdAt not set if not timestamp type', async t => {
  const client = connect({
    port
  })

  const league = await client.set({
    $language: 'en',
    type: 'league',
    title: 'yesh'
  })

  const result = await client.get({
    $language: 'en',
    $id: league,
    id: true,
    title: true,
    createdAt: true
  })

  t.deepEqual(result, {
    id: league,
    title: 'yesh'
  })

  await client.delete('root')
  await client.destroy()
})

test.serial('createdAt not set if provided in modify props', async t => {
  const client = connect({
    port
  })

  const match = await client.set({
    $language: 'en',
    type: 'match',
    title: 'yesh',
    createdAt: 12345
  })

  const result = await client.get({
    $language: 'en',
    $id: match,
    id: true,
    title: true,
    createdAt: true
  })

  t.deepEqual(result, {
    id: match,
    title: 'yesh',
    createdAt: 12345
  })

  await client.delete('root')
  await client.destroy()
})

test.serial('Set empty object', async t => {
  const client = connect({
    port
  })

  const id = await client.set({
    $id: 'maEmpty',
    nestedObj: {
      a: {},
      b: {}
    }
  })
  try {
    const result = await client.get({
      $id: id,
      $all: true
    })

    t.pass()
  } catch (e) {
    t.fail()
  }

  await client.delete('root')
  await client.destroy()
})

test.serial('no root in parents when adding nested', async t => {
  const client = connect({
    port
  })

  await client.set({
    $id: 'ma1',
    $language: 'en',
    children: {
      $add: [
        {
          $alias: 'hello',
          type: 'match',
          title: 'hello1'
        },
        {
          $alias: 'hello2',
          type: 'match',
          title: 'hello2',
          parents: ['root', 'ma1']
        }
      ]
    }
  })

  // await new Promise(res => setTimeout(res, 1e8))
  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'hello',
      parents: true,
      title: true
    }),
    {
      parents: ['ma1'],
      title: 'hello1'
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'hello2',
      parents: true,
      title: true
    }),
    {
      parents: ['root', 'ma1'],
      title: 'hello2'
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('can disable autoadding of root', async t => {
  const client = connect({
    port
  })

  const m1 = await client.set({
    type: 'match'
  })

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(DEFAULT_HIERARCHY, m1),
    ['root']
  )

  const m2 = await client.set({
    type: 'match',
    parents: { $noRoot: true }
  })

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(DEFAULT_HIERARCHY, m2),
    []
  )

  const m3 = await client.set({
    type: 'match',
    children: { $value: 'maMatch3', $noRoot: true }
  })

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(DEFAULT_HIERARCHY, 'maMatch3'),
    [m3]
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('createdAt not set if nothing changed', async t => {
  const client = connect({
    port
  })

  const before = Date.now()
  const person = await client.set({
    $language: 'en',
    type: 'person',
    title: 'yesh'
  })
  const after = Date.now()

  let result = await client.get({
    $language: 'en',
    $id: person,
    id: true,
    title: true,
    createdAt: true,
    updatedAt: true
  })

  let createdAt = result.createdAt
  let updatedAt = result.updatedAt
  delete result.createdAt
  delete result.updatedAt

  t.deepEqual(result, {
    id: person,
    title: 'yesh'
  })

  t.true(
    typeof createdAt === 'number' && createdAt <= after && createdAt >= before
  )

  t.deepEqual(createdAt, updatedAt)

  await client.set({
    $language: 'en',
    type: 'person',
    title: 'yesh',
    children: []
  })

  result = await client.get({
    $language: 'en',
    $id: person,
    id: true,
    title: true,
    createdAt: true,
    updatedAt: true
  })

  createdAt = result.createdAt
  updatedAt = result.updatedAt
  delete result.createdAt
  delete result.updatedAt

  t.true(
    typeof createdAt === 'number' && createdAt <= after && createdAt >= before
  )

  t.deepEqual(createdAt, updatedAt)

  await client.delete('root')
  await client.destroy()
})

test.serial('$delete: true', async t => {
  const client = connect(
    {
      port
    },
    { loglevel: 'info' }
  )

  const match = await client.set({
    type: 'match'
  })

  const root = await client.set({
    $id: 'root',
    value: 9001
  })

  t.deepEqual(root, 'root')
  t.deepEqual(await client.redis.selva_object_get('root', 'value'), '9001')
  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'),
    [match]
  )

  await client.set({
    $id: 'root',
    value: { $delete: true }
  })

  t.deepEqual(await client.redis.selva_object_exists('root', 'value'), 0)
  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'),
    [match]
  )

  await client.set({
    $id: 'maA',
    type: 'match',
    title: { en: 'yesh extra nice', de: 'ja extra nice' },
    obj: {
      hello: 'yes hello'
    },
    reffyRef: 'root',
    reffyRefs: ['root'],
    settySet: { $add: 'hmmmm' }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maA',
      id: true,
      title: true,
      obj: true,
      reffyRef: true,
      reffyRefs: true,
      settySet: true
    }),
    {
      id: 'maA',
      title: {
        en: 'yesh extra nice',
        de: 'ja extra nice'
      },
      obj: {
        hello: 'yes hello'
      },
      reffyRef: 'root',
      reffyRefs: ['root'],
      settySet: ['hmmmm']
    }
  )

  await client.set({
    $id: 'maA',
    title: { de: { $delete: true } }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maA',
      id: true,
      title: true,
      obj: true,
      reffyRef: true,
      reffyRefs: true,
      settySet: true
    }),
    {
      id: 'maA',
      title: {
        en: 'yesh extra nice'
      },
      obj: {
        hello: 'yes hello'
      },
      reffyRef: 'root',
      reffyRefs: ['root'],
      settySet: ['hmmmm']
    }
  )

  await client.set({
    $id: 'maA',
    obj: { hello: { $delete: true }, hallo: 'mmmmh' }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maA',
      id: true,
      title: true,
      obj: true,
      reffyRef: true,
      reffyRefs: true,
      settySet: true
    }),
    {
      id: 'maA',
      title: {
        en: 'yesh extra nice'
      },
      obj: {
        hallo: 'mmmmh'
      },
      reffyRef: 'root',
      reffyRefs: ['root'],
      settySet: ['hmmmm']
    }
  )

  await client.set({
    $id: 'maA',
    obj: { $delete: true }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maA',
      id: true,
      title: true,
      obj: true,
      reffyRef: true,
      reffyRefs: true,
      settySet: true
    }),
    {
      id: 'maA',
      title: {
        en: 'yesh extra nice'
      },
      reffyRef: 'root',
      reffyRefs: ['root'],
      settySet: ['hmmmm']
    }
  )

  await client.set({
    $id: 'maA',
    title: { $delete: true }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maA',
      id: true,
      title: true,
      obj: true,
      reffyRef: true,
      reffyRefs: true,
      settySet: true
    }),
    {
      id: 'maA',
      reffyRef: 'root',
      reffyRefs: ['root'],
      settySet: ['hmmmm']
    }
  )

  await client.set({
    $id: 'maA',
    reffyRef: { $delete: true },
    title: { en: 'yes title is back!!!' }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maA',
      id: true,
      title: true,
      obj: true,
      reffyRef: true,
      reffyRefs: true,
      settySet: true
    }),
    {
      id: 'maA',
      title: {
        en: 'yes title is back!!!'
      },
      reffyRefs: ['root'],
      settySet: ['hmmmm']
    }
  )

  await client.set({
    $id: 'maA',
    reffyRefs: { $delete: true }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maA',
      id: true,
      title: true,
      obj: true,
      reffyRef: true,
      reffyRefs: true,
      settySet: true
    }),
    {
      id: 'maA',
      title: {
        en: 'yes title is back!!!'
      },
      settySet: ['hmmmm']
    }
  )

  await client.set({
    $id: 'maA',
    settySet: { $delete: true }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maA',
      id: true,
      title: true,
      obj: true,
      reffyRef: true,
      reffyRefs: true,
      settySet: true
    }),
    {
      id: 'maA',
      title: {
        en: 'yes title is back!!!'
      }
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('deleting an object', async t => {
  const client = connect(
    {
      port
    },
    { loglevel: 'info' }
  )

  const match = await client.set({
    type: 'match',
    obj: {
      hello: 'hello',
      hallo: 'hallo'
    }
  })

  t.deepEqual(await client.get({ $id: match, obj: true }), {
    obj: {
      hello: 'hello',
      hallo: 'hallo'
    }
  })

  await client.set({
    $id: match,
    obj: { $delete: true }
  })

  t.deepEqual(await client.get({ $id: match, obj: true }), {})

  await client.delete('root')
  await client.destroy()
})
