import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number

const timeQuery = {
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
            $field: 'type',
          },
          {
            $value: 'now',
            $field: 'endTime',
            $operator: '<',
          },
        ],
      },
    },
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
            $field: 'type',
          },
          {
            $value: 'now',
            $field: 'startTime',
            $operator: '<',
          },
          {
            $value: 'now',
            $field: 'endTime',
            $operator: '>',
          },
        ],
      },
    },
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
            $field: 'type',
          },
          {
            $value: 'now',
            $field: 'startTime',
            $operator: '>',
          },
        ],
      },
    },
  },
}

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })

  await wait(500)

  const client = connect({ port })
  await client.updateSchema({
    languages: ['en', 'de', 'fr', 'it', 'nl'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
        },
      },
      team: {
        prefix: 'te',
        fields: {
          title: { type: 'text', search: { type: ['TEXT-LANGUAGE-SUG'] } },
          published: { type: 'boolean', search: { type: ['TAG'] } },
        },
      },
      video: {
        prefix: 'vi',
        fields: {
          title: { type: 'text', search: { type: ['TEXT-LANGUAGE-SUG'] } },
          published: { type: 'boolean', search: { type: ['TAG'] } },
        },
      },
      sport: {
        prefix: 'sp',
        fields: {
          title: { type: 'text', search: { type: ['TEXT-LANGUAGE-SUG'] } },
          published: { type: 'boolean', search: { type: ['TAG'] } },
        },
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
            search: { type: ['NUMERIC', 'SORTABLE'] },
          },
          endTime: {
            type: 'timestamp',
            search: { type: ['NUMERIC', 'SORTABLE'] },
          },
          date: {
            type: 'timestamp',
            search: { type: ['NUMERIC', 'SORTABLE'] },
          },
          fun: { type: 'set', items: { type: 'string' } },
          related: { type: 'references', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC'] } },
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

test.serial('one client', async (t) => {
  const client = connect(
    async () => {
      await wait(10)
      return { port }
    },
    { loglevel: 'info' }
  )

  await wait(5)

  client
    .observe({
      $id: 'maTest',
      title: true,
    })
    .subscribe((r) => {
      // console.log('result:', r)
    })

  await wait(500)

  client.set({
    $id: 'maTest',
    title: {
      de: 'Gutentag',
    },
  })

  await wait(500)
  await client.delete('root')
  await client.destroy()

  t.true(true)
})

test.serial('many async clients - timebased query', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  const now = Date.now()

  await client.set({
    type: 'match',
    $id: 'ma1',
    name: 'upcoming match',
    startTime: now + 2000, // 2 sec from now
    endTime: now + 5000, // 5 sec from now
  })

  let k = 10
  let result
  while (k--) {
    const client = connect(async () => {
      await wait(10)
      return { port }
    })

    client.observe(timeQuery).subscribe((r) => {
      // console.log('time:', r)
      result = r
    })
  }

  await wait(500)
  t.deepEqualIgnoreOrder(result, {
    upcoming: [{ id: 'ma1' }],
    past: [],
    live: [],
  })
  await wait(3000)
  t.deepEqualIgnoreOrder(result, {
    upcoming: [],
    past: [],
    live: [{ id: 'ma1' }],
  })
  await wait(3000)
  t.deepEqualIgnoreOrder(result, {
    upcoming: [],
    past: [{ id: 'ma1' }],
    live: [],
  })
  await wait(10000)
  await client.delete('root')
  await client.destroy()

  t.pass()
})
