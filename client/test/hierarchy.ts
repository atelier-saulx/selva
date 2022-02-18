import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
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
              poster: { type: 'string' },
            },
          },
        },
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
              poster: { type: 'string' },
            },
          },
        },
      },
      team: {
        hierarchy: {
          club: { excludeAncestryWith: ['video'] },
        },
        prefix: 'te',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
      club: {
        prefix: 'cl',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
      match: {
        prefix: 'ma',
        hierarchy: {
          team: {
            excludeAncestryWith: ['league'],
          },
          video: false,
          club: { excludeAncestryWith: ['video'] },
          person: { includeAncestryWith: ['family'] },
          $default: { excludeAncestryWith: ['vehicle', 'video'] },
        },
        fields: {
          title: { type: 'text' },
          value: { type: 'number' },
          description: { type: 'text' },
        },
      },
      person: {
        prefix: 'pe',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
      vehicle: {
        prefix: 've',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
      video: {
        prefix: 'vi',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
      family: {
        prefix: 'fa',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
    },
  })

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial.skip(
  'inheriting while excluding league from match through setting match as parent',
  async (t) => {
    const client = connect({ port }, { loglevel: 'info' })

    const league = await client.set({
      type: 'league',
      title: { en: 'league1' },
      value: 90,
    })

    const season = await client.set({
      type: 'season',
      title: { en: 'season1' },
      parents: [league],
      value: 12,
    })

    const club = await client.set({
      type: 'club',
      title: { en: 'club1' },
      value: 22,
    })

    const team = await client.set({
      type: 'team',
      title: { en: 'team1' },
      parents: [club, season],
    })

    const match = await client.set({
      type: 'match',
      title: { en: 'match1' },
      parents: [team],
    })

    // check inheritance
    t.deepEqual(
      await client.get({
        $id: match,
        id: true,
        title: true,
        value: { $inherit: true },
      }),
      {
        id: match,
        title: { en: 'match1' },
        value: 22,
      }
    )

    const ancestors = (id) =>
      client.redis.selva_hierarchy_find(
        '',
        '___selva_hierarchy',
        'ancestors',
        id
      )

    // check ancestors
    t.deepEqualIgnoreOrder(await ancestors(league), ['root'])
    t.deepEqualIgnoreOrder(await ancestors(season), ['root', league])
    t.deepEqualIgnoreOrder(await ancestors(club), ['root'])
    t.deepEqualIgnoreOrder(await ancestors(team), [
      'root',
      club,
      season,
      league,
    ])
    t.deepEqualIgnoreOrder(await ancestors(match), [team, club, 'root'])

    const r = await client.get({
      $id: match,
      $rawAncestors: true,
    })

    t.deepEqualIgnoreOrder(r.rawAncestors, [league, club, 'root', team, season])

    // cleanup
    await client.delete('root')
    await client.destroy()
  }
)

test.serial.skip(
  'inheriting while excluding league from match through setting match as child',
  async (t) => {
    const client = connect({ port })

    const match = await client.set({
      type: 'match',
      title: { en: 'match1' },
    })

    const team = await client.set({
      type: 'team',
      title: { en: 'team1' },
      children: [match],
    })

    const club = await client.set({
      type: 'club',
      title: { en: 'club1' },
      children: [team],
      value: 22,
    })

    const season = await client.set({
      type: 'season',
      title: { en: 'season1' },
      children: [team],
      value: 12,
    })

    // club -> team -> league -> season
    // season -> team
    // season -> match
    // trying to inherit layout
    const league = await client.set({
      type: 'league',
      title: { en: 'league1' },
      children: [season],
      value: 90,
    })

    // check inheritance
    t.deepEqual(
      await client.get({
        $id: match,
        id: true,
        title: true,
        value: { $inherit: true },
      }),
      {
        id: match,
        title: { en: 'match1' },
        value: 22,
      }
    )

    const ancestors = (id) =>
      client.redis.selva_hierarchy_find(
        '',
        '___selva_hierarchy',
        'ancestors',
        id
      )

    // check ancestors
    t.deepEqualIgnoreOrder(await ancestors(league), ['root'])
    t.deepEqualIgnoreOrder(await ancestors(season), ['root', league])
    t.deepEqualIgnoreOrder(await ancestors(club), ['root'])
    t.deepEqualIgnoreOrder(await ancestors(team), [
      'root',
      club,
      season,
      league,
    ])
    t.deepEqualIgnoreOrder(await ancestors(match), [team, club, 'root'])

    // cleanup
    await client.delete('root')
    await client.destroy()
  }
)

test.serial('more complex hierarchies', async (t) => {
  const client = connect({ port })

  const match = await client.set({
    type: 'match',
    title: { en: 'match1' },
  })

  const porche = await client.set({
    type: 'vehicle',
    title: { en: 'extra nice porche' },
  })

  const coownedFerrari = await client.set({
    type: 'vehicle',
    title: { en: 'vroomvroom' },
  })

  const player1 = await client.set({
    type: 'person',
    title: { en: 'player1' },
    children: [match, porche],
  })

  const player2 = await client.set({
    type: 'person',
    title: { en: 'player2' },
    children: [match, coownedFerrari],
  })

  const player = await client.set({
    type: 'person',
    title: { en: 'team2 player' },
    children: [match],
  })

  const player3 = await client.set({
    type: 'person',
    title: { en: 'player3' },
    children: [match, coownedFerrari],
  })

  const team = await client.set({
    type: 'team',
    title: { en: 'team1' },
    children: [match],
  })

  const team2 = await client.set({
    type: 'team',
    title: { en: 'team2' },
    children: [player],
  })

  const club = await client.set({
    type: 'club',
    title: { en: 'club1' },
    children: [team],
    value: 22,
  })

  const weirdCommercial = await client.set({
    type: 'video',
    title: { en: 'random stuff in video' },
    children: [player2, player3, match, club],
  })

  const teamCommercial = await client.set({
    type: 'video',
    title: { en: 'random stuff in video' },
    children: [match],
  })

  const family1 = await client.set({
    type: 'family',
    title: { en: 'family1' },
    children: [player1, player2, porche, coownedFerrari],
  })

  const family2 = await client.set({
    type: 'family',
    title: { en: 'family2' },
    children: [player, player3], // ferrari not family car
  })

  const season = await client.set({
    type: 'season',
    title: { en: 'season1' },
    children: [team],
    value: 12,
  })

  const league = await client.set({
    type: 'league',
    title: { en: 'league1' },
    children: [season],
    value: 90,
  })

  // check ancestors
  // families
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'ancestors',
      family1
    ),
    ['root']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'ancestors',
      family2
    ),
    ['root']
  )

  // players
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'ancestors',
      player1
    ),
    ['root', family1]
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'ancestors',
      player2
    ),
    ['root', family1, weirdCommercial]
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'ancestors',
      player3
    ),
    ['root', family2, weirdCommercial]
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'ancestors',
      player
    ),
    ['root', family2, team2]
  )

  // cars
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'ancestors',
      porche
    ),
    ['root', family1, player1]
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'ancestors',
      coownedFerrari
    ),
    ['root', family1, family2, player2, player3, weirdCommercial]
  )

  // league
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'ancestors',
      league
    ),
    ['root']
  )

  // season
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'ancestors',
      season
    ),
    ['root', league]
  )

  // club
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'ancestors',
      club
    ),
    ['root', weirdCommercial]
  )

  // teams
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'ancestors',
      team
    ),
    ['root', season, league, club, weirdCommercial]
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'ancestors',
      team2
    ),
    ['root']
  )

  // cleanup
  await client.delete('root')
  await client.destroy()
})

test.serial(
  'inheriting item while excluding league from match through setting match as child',
  async (t) => {
    const client = connect({ port })

    const match = await client.set({
      type: 'match',
      title: { en: 'match1' },
    })

    const team = await client.set({
      type: 'team',
      title: { en: 'team1' },
      children: [match],
    })

    const club = await client.set({
      type: 'club',
      title: { en: 'club1' },
      children: [team],
      value: 22,
    })

    const season = await client.set({
      type: 'season',
      title: { en: 'season1' },
      children: [team],
      value: 12,
    })

    // club -> team -> league -> season
    // season -> team
    // season -> match
    // trying to inherit layout
    const league = await client.set({
      type: 'league',
      title: { en: 'league1' },
      children: [season],
      value: 90,
    })

    // check inheritance
    t.deepEqual(
      await client.get({
        $id: match,
        id: true,
        title: true,
        thing: {
          id: true,
          $inherit: { $item: ['club', 'season'] },
        },
      }),
      {
        id: match,
        title: { en: 'match1' },
        thing: {
          id: club,
        },
      }
    )

    // check ancestors
    t.deepEqualIgnoreOrder(
      await client.redis.selva_hierarchy_find(
        '',
        '___selva_hierarchy',
        'ancestors',
        league
      ),
      ['root']
    )
    t.deepEqualIgnoreOrder(
      await client.redis.selva_hierarchy_find(
        '',
        '___selva_hierarchy',
        'ancestors',
        season
      ),
      ['root', league]
    )
    t.deepEqualIgnoreOrder(
      await client.redis.selva_hierarchy_find(
        '',
        '___selva_hierarchy',
        'ancestors',
        club
      ),
      ['root']
    )
    t.deepEqualIgnoreOrder(
      await client.redis.selva_hierarchy_find(
        '',
        '___selva_hierarchy',
        'ancestors',
        team
      ),
      ['root', club, season, league]
    )
    t.deepEqualIgnoreOrder(
      await client.redis.selva_hierarchy_find(
        '',
        '___selva_hierarchy',
        'ancestors',
        club
      ),
      ['root']
    )

    // cleanup
    await client.delete('root')
    await client.destroy()
  }
)
