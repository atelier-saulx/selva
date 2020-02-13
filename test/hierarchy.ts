import test from 'ava'
import { connect } from '../client/src/index'
import { start } from '../server/src/index'
import { wait } from './assertions'
import './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })

  const client = connect({ port })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          value: { type: 'number' },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      season: {
        prefix: 'se',
        fields: {
          value: { type: 'number' },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      team: {
        hierarchy: {
          club: { excludeAncestryWith: ['video'] }
        },
        prefix: 'te',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      club: {
        prefix: 'cl',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      match: {
        prefix: 'ma',
        hierarchy: {
          team: {
            excludeAncestryWith: ['league']
          },
          video: false,
          club: { excludeAncestryWith: ['video'] },
          person: { includeAncestryWith: ['family'] },
          $default: { excludeAncestryWith: ['vehicle', 'video'] }
        },
        fields: {
          title: { type: 'text' },
          value: { type: 'number' },
          description: { type: 'text' }
        }
      },
      person: {
        prefix: 'pe',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      vehicle: {
        prefix: 've',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      video: {
        prefix: 'vi',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      family: {
        prefix: 'fa',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
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
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial(
  'inheriting while excluding league from match through setting match as parent',
  async t => {
    const client = connect({ port })

    const league = await client.set({
      type: 'league',
      title: { en: 'league1' },
      value: 90
    })

    const season = await client.set({
      type: 'season',
      title: { en: 'season1' },
      parents: [league],
      value: 12
    })

    const club = await client.set({
      type: 'club',
      title: { en: 'club1' },
      value: 22
    })

    const team = await client.set({
      type: 'team',
      title: { en: 'team1' },
      parents: [club, season]
    })

    const match = await client.set({
      type: 'match',
      title: { en: 'match1' },
      parents: [team]
    })

    // check inheritance
    t.deepEqual(
      await client.get({
        $id: match,
        id: true,
        title: true,
        value: { $inherit: true }
      }),
      {
        id: match,
        title: { en: 'match1' },
        value: 22
      }
    )

    // check ancestors
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(league + '.ancestors', 0, -1),
      ['root']
    )
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(season + '.ancestors', 0, -1),
      ['root', league]
    )
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(club + '.ancestors', 0, -1),
      ['root']
    )
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(team + '.ancestors', 0, -1),
      ['root', club, season, league]
    )
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(match + '.ancestors', 0, -1),
      [team, club, 'root']
    )

    // cleanup
    await client.delete('root')
    await client.destroy()
  }
)

test.serial('ancestry has only one season in real world setting', async t => {
  const client = connect({ port })

  const match1 = await client.set({
    type: 'match',
    title: { en: 'match1' }
  })

  const match2 = await client.set({
    type: 'match',
    title: { en: 'match2' }
  })

  const match3 = await client.set({
    type: 'match',
    title: { en: 'match3' }
  })

  const match4 = await client.set({
    type: 'match',
    title: { en: 'match4' }
  })

  const team1 = await client.set({
    type: 'team',
    title: { en: 'team1' },
    children: [match1, match2, match3, match4]
  })

  const team2 = await client.set({
    type: 'team',
    title: { en: 'team2' },
    children: [match1, match2, match3, match4]
  })

  const season1 = await client.set({
    type: 'season',
    title: { en: 'season1' },
    children: [team1, team2, match1],
    value: 12
  })

  const season2 = await client.set({
    type: 'season',
    title: { en: 'season1' },
    children: [team1, team2, match2],
    value: 12
  })

  const season3 = await client.set({
    type: 'season',
    title: { en: 'season3' },
    children: [team1, team2, match3],
    value: 12
  })

  const season4 = await client.set({
    type: 'season',
    title: { en: 'season4' },
    children: [team1, team2, match4],
    value: 12
  })

  const league1 = await client.set({
    type: 'league',
    title: { en: 'league1' },
    children: [season1, season2],
    value: 90
  })

  const league2 = await client.set({
    type: 'league',
    title: { en: 'league1' },
    children: [season3, season4],
    value: 90
  })

  t.deepEqualIgnoreOrder(
    await client.redis.zrange(match1 + '.ancestors', 0, -1),
    ['root', league1, season1, team1, team2]
  )

  t.deepEqualIgnoreOrder(
    await client.redis.zrange(match2 + '.ancestors', 0, -1),
    ['root', league1, season2, team1, team2]
  )

  t.deepEqualIgnoreOrder(
    await client.redis.zrange(match3 + '.ancestors', 0, -1),
    ['root', league2, season3, team1, team2]
  )

  t.deepEqualIgnoreOrder(
    await client.redis.zrange(match4 + '.ancestors', 0, -1),
    ['root', league2, season4, team1, team2]
  )

  // cleanup
  await client.delete('root')
  await client.destroy()
})

test.serial(
  'inheriting while excluding league from match through setting match as child',
  async t => {
    const client = connect({ port })

    const match = await client.set({
      type: 'match',
      title: { en: 'match1' }
    })

    const team = await client.set({
      type: 'team',
      title: { en: 'team1' },
      children: [match]
    })

    const club = await client.set({
      type: 'club',
      title: { en: 'club1' },
      children: [team],
      value: 22
    })

    const season = await client.set({
      type: 'season',
      title: { en: 'season1' },
      children: [team],
      value: 12
    })

    // club -> team -> league -> season
    // season -> team
    // season -> match
    // trying to inherit layout
    const league = await client.set({
      type: 'league',
      title: { en: 'league1' },
      children: [season],
      value: 90
    })

    // check inheritance
    t.deepEqual(
      await client.get({
        $id: match,
        id: true,
        title: true,
        value: { $inherit: true }
      }),
      {
        id: match,
        title: { en: 'match1' },
        value: 22
      }
    )

    // check ancestors
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(league + '.ancestors', 0, -1),
      ['root']
    )
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(season + '.ancestors', 0, -1),
      ['root', league]
    )
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(club + '.ancestors', 0, -1),
      ['root']
    )
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(team + '.ancestors', 0, -1),
      ['root', club, season, league]
    )
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(match + '.ancestors', 0, -1),
      [team, club, 'root']
    )

    // cleanup
    await client.delete('root')
    await client.destroy()
  }
)

test.serial('more complex hierarchies', async t => {
  const client = connect({ port })

  const match = await client.set({
    type: 'match',
    title: { en: 'match1' }
  })

  const porche = await client.set({
    type: 'vehicle',
    title: { en: 'extra nice porche' }
  })

  const coownedFerrari = await client.set({
    type: 'vehicle',
    title: { en: 'vroomvroom' }
  })

  const player1 = await client.set({
    type: 'person',
    title: { en: 'player1' },
    children: [match, porche]
  })

  const player2 = await client.set({
    type: 'person',
    title: { en: 'player2' },
    children: [match, coownedFerrari]
  })

  const player = await client.set({
    type: 'person',
    title: { en: 'team2 player' },
    children: [match]
  })

  const player3 = await client.set({
    type: 'person',
    title: { en: 'player3' },
    children: [match, coownedFerrari]
  })

  const team = await client.set({
    type: 'team',
    title: { en: 'team1' },
    children: [match]
  })

  const team2 = await client.set({
    type: 'team',
    title: { en: 'team2' },
    children: [player]
  })

  const club = await client.set({
    type: 'club',
    title: { en: 'club1' },
    children: [team],
    value: 22
  })

  const weirdCommercial = await client.set({
    type: 'video',
    title: { en: 'random stuff in video' },
    children: [player2, player3, match, club]
  })

  const teamCommercial = await client.set({
    type: 'video',
    title: { en: 'random stuff in video' },
    children: [match]
  })

  const family1 = await client.set({
    type: 'family',
    title: { en: 'family1' },
    children: [player1, player2, porche, coownedFerrari]
  })

  const family2 = await client.set({
    type: 'family',
    title: { en: 'family2' },
    children: [player, player3] // ferrari not family car
  })

  const season = await client.set({
    type: 'season',
    title: { en: 'season1' },
    children: [team],
    value: 12
  })

  const league = await client.set({
    type: 'league',
    title: { en: 'league1' },
    children: [season],
    value: 90
  })

  // check ancestors
  // families
  t.deepEqualIgnoreOrder(
    await client.redis.zrange(family1 + '.ancestors', 0, -1),
    ['root']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.zrange(family2 + '.ancestors', 0, -1),
    ['root']
  )

  // players
  t.deepEqualIgnoreOrder(
    await client.redis.zrange(player1 + '.ancestors', 0, -1),
    ['root', family1]
  )
  t.deepEqualIgnoreOrder(
    await client.redis.zrange(player2 + '.ancestors', 0, -1),
    ['root', family1, weirdCommercial]
  )
  t.deepEqualIgnoreOrder(
    await client.redis.zrange(player3 + '.ancestors', 0, -1),
    ['root', family2, weirdCommercial]
  )
  t.deepEqualIgnoreOrder(
    await client.redis.zrange(player + '.ancestors', 0, -1),
    ['root', family2, team2]
  )

  // cars
  t.deepEqualIgnoreOrder(
    await client.redis.zrange(porche + '.ancestors', 0, -1),
    ['root', family1, player1]
  )
  t.deepEqualIgnoreOrder(
    await client.redis.zrange(coownedFerrari + '.ancestors', 0, -1),
    ['root', family1, family2, player2, player3, weirdCommercial]
  )

  // league
  t.deepEqualIgnoreOrder(
    await client.redis.zrange(league + '.ancestors', 0, -1),
    ['root']
  )

  // season
  t.deepEqualIgnoreOrder(
    await client.redis.zrange(season + '.ancestors', 0, -1),
    ['root', league]
  )

  // club
  t.deepEqualIgnoreOrder(
    await client.redis.zrange(club + '.ancestors', 0, -1),
    ['root', weirdCommercial]
  )

  // teams
  t.deepEqualIgnoreOrder(
    await client.redis.zrange(team + '.ancestors', 0, -1),
    ['root', season, league, club]
  )
  t.deepEqualIgnoreOrder(
    await client.redis.zrange(team2 + '.ancestors', 0, -1),
    ['root']
  )

  // match
  t.deepEqualIgnoreOrder(
    await client.redis.zrange(match + '.ancestors', 0, -1),
    [
      'root',
      club,
      team,
      player,
      player1,
      player2,
      family1,
      family2,
      player3,
      weirdCommercial
    ]
  )

  // cleanup
  await client.delete('root')
  await client.destroy()
})

test.serial(
  'inheriting item while excluding league from match through setting match as child',
  async t => {
    const client = connect({ port })

    const match = await client.set({
      type: 'match',
      title: { en: 'match1' }
    })

    const team = await client.set({
      type: 'team',
      title: { en: 'team1' },
      children: [match]
    })

    const club = await client.set({
      type: 'club',
      title: { en: 'club1' },
      children: [team],
      value: 22
    })

    const season = await client.set({
      type: 'season',
      title: { en: 'season1' },
      children: [team],
      value: 12
    })

    // club -> team -> league -> season
    // season -> team
    // season -> match
    // trying to inherit layout
    const league = await client.set({
      type: 'league',
      title: { en: 'league1' },
      children: [season],
      value: 90
    })

    // check inheritance
    t.deepEqual(
      await client.get({
        $id: match,
        id: true,
        title: true,
        thing: {
          id: true,
          $inherit: { $item: ['club', 'season'] }
        }
      }),
      {
        id: match,
        title: { en: 'match1' },
        thing: {
          id: club
        }
      }
    )

    // check ancestors
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(league + '.ancestors', 0, -1),
      ['root']
    )
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(season + '.ancestors', 0, -1),
      ['root', league]
    )
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(club + '.ancestors', 0, -1),
      ['root']
    )
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(team + '.ancestors', 0, -1),
      ['root', club, season, league]
    )
    t.deepEqualIgnoreOrder(
      await client.redis.zrange(match + '.ancestors', 0, -1),
      [team, club, 'root']
    )

    // cleanup
    await client.delete('root')
    await client.destroy()
  }
)
