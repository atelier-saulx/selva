import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'

let srv

test.before(async t => {
  srv = await start({
    port: 8082,
    developmentLogging: true,
    loglevel: 'info'
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })

  const client = connect({ port: 8082 })
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
          team: { excludeAncestryWith: ['league'] },
          video: false,
          person: { includeAncestryWith: ['family'] },
          $default: { excludeAncestryWith: ['vehicle'] }
        },
        fields: {
          title: { type: 'text' },
          value: { type: 'number' },
          description: { type: 'text' }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port: 8082 })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial(
  'inheriting while excluding league from match through setting match as child',
  async t => {
    const client = connect({ port: 8082 })

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

    console.log(
      `ancestors of team`,
      await client.redis.zrange(team + '.ancestors', 0, -1)
    )

    console.log(
      `ancestors of match`,
      await client.redis.zrange(match + '.ancestors', 0, -1)
    )

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

test.serial(
  'inheriting while excluding league from match through setting match as parent',
  async t => {
    const client = connect({ port: 8082 })

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

    // club -> team -> league -> season
    // season -> team
    // season -> match
    // trying to inherit layout
    console.log(
      `ancestors of team`,
      await client.redis.zrange(team + '.ancestors', 0, -1)
    )

    console.log(
      `ancestors of match`,
      await client.redis.zrange(match + '.ancestors', 0, -1)
    )

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
    // await client.delete('root')
    await client.destroy()
  }
)

test.serial(
  'ancestry has only one team when match in season and league',
  async t => {
    const client = connect({ port: 8082 })

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
  }
)
