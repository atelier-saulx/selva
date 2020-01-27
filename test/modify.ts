import test, { ExecutionContext } from 'ava'
import './assertions'
import { connect, SelvaClient } from '../src/index'
import { start } from 'selva-server'
import { dumpDb, idExists } from './assertions'

function ancestorEquals(t: ExecutionContext, a: string, b: string): boolean {
  const splitA = a.split(',')
  const splitB = b.split(',')

  return t.deepEqualIgnoreOrder(splitA, splitB)
}

let srv
test.before(async t => {
  srv = await start({
    port: 6061,
    // developmentLogging: true,
    loglevel: 'info'
  })

  await new Promise((resolve, reject) => {
    setTimeout(resolve, 200)
  })

  const client = connect({
    port: 6061
  })

  await client.updateSchema({
    languages: ['en', 'nl', 'de'],
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: {
            type: 'text'
          }
        }
      },
      league: {
        prefix: 'cu',
        fields: {
          title: {
            type: 'text'
          }
        }
      },
      person: {
        prefix: 'pe',
        fields: {
          title: {
            type: 'text'
          }
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

test.after(async _t => {
  const client = connect({ port: 6061 })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('basic', async t => {
  const client = connect({
    port: 6061
  })

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
    await client.redis.smembers(match + '.children'),
    [person],
    'match has correct children'
  )

  t.deepEqual(
    (await client.redis.smembers('root.children')).sort(),
    [league, match].sort(),
    'root has correct children'
  )

  t.deepEqual(
    await client.redis.smembers(league + '.children'),
    [],
    'league has no children'
  )

  t.is(
    await client.redis.hget(person, 'title.en'),
    'flurpy man',
    'Title of person is correctly set'
  )

  ancestorEquals(t, await client.redis.hget(match, 'ancestors'), 'root')

  ancestorEquals(t, await client.redis.hget(league, 'ancestors'), 'root')

  // move person from match to league
  await client.set({
    $id: person,
    parents: [league]
  })

  t.deepEqual(
    await client.redis.smembers(league + '.children'),
    [person],
    'league has person after move'
  )

  t.deepEqual(
    await client.redis.smembers(match + '.children'),
    [],
    'match has no children after move'
  )

  ancestorEquals(
    t,
    await client.redis.hget(person, 'ancestors'),
    ['root', league].join(',')
  )

  // add extra parent using $add
  await client.set({
    $id: person,
    parents: {
      $add: match
    }
  })

  t.deepEqual(
    await client.redis.smembers(match + '.children'),
    [person],
    'match has children after $add'
  )

  t.deepEqual(
    (await client.redis.smembers(person + '.parents')).sort(),
    [league, match].sort(),
    'person has correct parents after $add'
  )

  ancestorEquals(
    t,
    await client.redis.hget(person, 'ancestors'),
    ['root', league, match].join(',')
  )

  // remove league from person
  await client.set({
    $id: person,
    parents: {
      $delete: league
    }
  })

  t.deepEqual(
    await client.redis.smembers(league + '.children'),
    [],
    'league has no children after $delete'
  )

  t.deepEqual(
    await client.redis.smembers(person + '.parents'),
    [match],
    'person has correct parents after $delete'
  )

  ancestorEquals(
    t,
    await client.redis.hget(person, 'ancestors'),
    ['root', match].join(',')
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
    await client.redis.smembers(match + '.children'),
    [person],
    'match has children after 2nd $add'
  )

  t.deepEqual(
    (await client.redis.smembers(person + '.parents')).sort(),
    [league, match].sort(),
    'person has correct parents after 2nd $add'
  )

  ancestorEquals(
    t,
    await client.redis.hget(person, 'ancestors'),
    ['root', match, league].join(',')
  )

  // reset children
  await client.set({
    $id: match,
    children: []
  })

  t.deepEqual(
    await client.redis.smembers(match + '.children'),
    [],
    'match has no children after reset'
  )

  t.deepEqual(
    await client.redis.smembers(person + '.parents'),
    [league],
    'person has correct parents after reset of children of match'
  )

  ancestorEquals(
    t,
    await client.redis.hget(person, 'ancestors'),
    ['root', league].join(',')
  )

  // add person to match using children
  await client.set({
    $id: match,
    children: [person]
  })

  t.deepEqual(
    await client.redis.smembers(match + '.children'),
    [person],
    'match has children after adding person to match using children'
  )

  t.deepEqual(
    (await client.redis.smembers(person + '.parents')).sort(),
    [league, match].sort(),
    'person has correct parents after adding person to match using children'
  )

  ancestorEquals(
    t,
    await client.redis.hget(person, 'ancestors'),
    ['root', league, match].join(',')
  )

  // add match to league using $add
  await client.set({
    $id: league,
    children: { $add: match }
  })

  t.deepEqual(
    (await client.redis.smembers(match + '.parents')).sort(),
    ['root', league].sort(),
    'match has correct parents after adding match as a child to league'
  )

  t.deepEqual(
    (await client.redis.smembers(league + '.children')).sort(),
    [match, person].sort(),
    'league has correct children after setting ancestors'
  )

  ancestorEquals(
    t,
    await client.redis.hget(person, 'ancestors'),
    ['root', league, match].join(',')
  )

  ancestorEquals(
    t,
    await client.redis.hget(match, 'ancestors'),
    ['root', league].join(',')
  )

  // delete match from league
  await client.set({
    $id: league,
    children: { $delete: match }
  })

  t.deepEqual(
    (await client.redis.smembers(person + '.parents')).sort(),
    [league, match].sort(),
    'person has correct parents after removing match from league'
  )

  ancestorEquals(
    t,
    await client.redis.hget(person, 'ancestors'),
    ['root', league, match].join(',')
  )

  t.deepEqual(
    await client.redis.smembers(match + '.parents'),
    ['root'].sort(),
    'match has correct parents after removing match from league'
  )

  t.is(
    await client.redis.hget(match, 'ancestors'),
    ['root'].join(','),
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
  t.deepEqual(await dumpDb(client), [])

  await client.destroy()
})

test.serial('deep hierarchy manipulation', async t => {
  const client = connect({
    port: 6061
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

  ancestorEquals(t, await client.redis.hget('cuB', 'ancestors'), 'root,cuX,cuA')
  ancestorEquals(t, await client.redis.hget('cuC', 'ancestors'), 'root,cuX,cuA')
  ancestorEquals(t, await client.redis.hget('cuD', 'ancestors'), 'root,cuX,cuA')
  ancestorEquals(
    t,
    await client.redis.hget('cuE', 'ancestors'),
    'root,cuX,cuA,cuD'
  )

  await client.set({
    $id: 'cuD',
    parents: { $delete: 'cuA' }
  })

  ancestorEquals(t, await client.redis.hget('cuD', 'ancestors'), 'root')
  ancestorEquals(t, await client.redis.hget('cuE', 'ancestors'), 'root,cuD')
})

test.serial('array, json and set', async t => {
  const client = connect({
    port: 6061
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
                // need to check if you are allready
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
                gurk: { type: 'string' },
                flap: { type: 'digest' }
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
        gurk: 'hello',
        flap: 'smurpy'
      }
    ]
  })
  const r = JSON.parse(await client.redis.hget(id, 'flap'))
  t.deepEqual(r, [
    {
      gurk: 'hello',
      flap: '6734082360af7f0c5aef4123f43abc44c4fbf19e8b251a316d7b9da95fde448e'
    }
  ])
})

test.serial('$increment, $default', async t => {
  const client = connect({
    port: 6061
  })
  await client.set({
    $id: 'viDingDong',
    value: {
      $default: 100,
      $increment: 10
    }
  })

  t.is(
    await client.redis.hget('viDingDong', 'value'),
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
    await client.redis.hget('viDingDong', 'value'),
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
    await client.redis.hget('viDingDong', 'title.en'),
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
    await client.redis.hget('viDingDong', 'title.en'),
    'title',
    'does not overwrite if value exists'
  )

  await client.delete('root')

  client.destroy()
})

test.serial('$merge = false', async t => {
  const client = connect({
    port: 6061
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

  t.is(await client.redis.hget('arPower', 'title.en'), 'flap')
  t.is(await client.redis.hget('arPower', 'title.de'), 'flurpels')

  await client.set({
    $id: 'arPower',
    $merge: false,
    title: {
      de: 'deutschland'
    }
  })

  t.is(await client.redis.hget('arPower', 'title.en'), null)
  t.is(await client.redis.hget('arPower', 'title.de'), 'deutschland')

  await client.set({
    $id: 'arPower',
    title: {
      $merge: false,
      nl: 'nl'
    }
  })

  t.is(await client.redis.hget('arPower', 'title.nl'), 'nl')
  t.is(await client.redis.hget('arPower', 'title.de'), null)

  await client.set({
    $id: 'arPower',
    image: {
      $merge: false,
      poster: 'x'
    }
  })

  t.is(await client.redis.hget('arPower', 'image.thumb'), null)

  await client.delete('root')
})

test.serial('automatic child creation', async t => {
  const client = connect({
    port: 6061
  })

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

  const children = await client.redis.smembers(parent + '.children')
  t.is(children.length, 3, 'Should have 3 children created')

  const titles = (
    await Promise.all(
      children.map(child => {
        return client.redis.hget(child, 'title.nl')
      })
    )
  ).sort()
  for (let i = 0; i < titles.length; i++) {
    t.is(titles[i], 'child' + (i + 1), `Child ${i} title should match`)
  }
})

// test.serial('Reference field', async t => {
//   const client = connect({
//     port: 6061
//   })

//   client.set({
//     $id: 'cuA',
//     layout: {
//       match: { components: [{ type: 'List', props: { x: true } }] },
//       custom: { $field: 'layout.match' },
//       video: { $field: 'layout.$type' }
//     }
//   })

//   await client.delete('root')
// })
