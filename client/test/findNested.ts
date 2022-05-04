import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })

  const client = connect({ port }, { loglevel: 'info' })
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {},
      },
      team: {
        prefix: 'te',
        fields: {
          value: { type: 'number' },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          value: { type: 'number' },
          status: { type: 'number' },
        },
      },
      thing: {
        prefix: 'th',
        fields: {
          docs: { type: 'references' },
        },
      },
      file: {
        prefix: 'tx',
        fields: {
          name: { type: 'string' },
          mirrors: { type: 'references' },
        },
      },
      mirror: {
        prefix: 'sp',
        fields: {
          url: { type: 'string' },
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

test.serial('get nested results', async (t) => {
  const client = connect({ port })

  const matches = []
  const teams = []

  for (let i = 0; i < 100; i++) {
    teams.push({
      $id: await client.id({ type: 'team' }),
      name: 'team ' + i,
      type: 'team',
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
          teams[~~(Math.random() * teams.length)].$id,
        ],
      },
      status: i < 5 ? 100 : 300,
    })
  }

  await Promise.all(teams.map((t) => client.set(t)))

  await client.set({
    type: 'league',
    name: 'league 1',
    children: matches,
  })

  const result = await client.get({
    $includeMeta: true,
    items: {
      name: true,
      id: true,
      teams: {
        id: true,
        name: true,
        flurpy: true,
        $list: {
          $find: {
            $traverse: 'ancestors',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'team',
              },
              {
                $field: 'value',
                $operator: '!=',
                $value: 2,
              },
            ],
          },
        },
      },
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
          ],
        },
      },
    },
  })

  t.is(result.items.length, 10, 'items length')
  t.is(result.items[0].teams.length, 2, 'has teams')

  await client.delete('root')
  await client.destroy()
})

test.serial('get nested results with $all', async (t) => {
  const client = connect({ port })

  const matches = []
  const teams = []

  for (let i = 0; i < 100; i++) {
    teams.push({
      $id: await client.id({ type: 'team' }),
      name: 'team ' + i,
      type: 'team',
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
          teams[~~(Math.random() * teams.length)].$id,
        ],
      },
      status: i < 5 ? 100 : 300,
    })
  }

  await Promise.all(teams.map((t) => client.set(t)))

  await client.set({
    type: 'league',
    name: 'league 1',
    children: matches,
  })

  let result = await client.get({
    items: {
      $all: true,
      teams: {
        $all: true,
        $list: {
          $find: {
            $traverse: 'ancestors',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'team',
              },
              {
                $field: 'value',
                $operator: '!=',
                $value: 2,
              },
            ],
          },
        },
      },
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
          ],
        },
      },
    },
  })

  t.is(result.items.length, 10, 'items length')
  t.is(result.items[0].teams.length, 2, 'has teams')

  result = await client.get({
    items: {
      $all: true,
      status: false,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
          ],
        },
      },
    },
  })

  t.is(result.items.length, 10, 'items length')
  t.assert(
    (<any[]>result.items).every((r) => {
      return typeof r.status === 'undefined'
    })
  )

  await client.delete('root')
  await client.destroy()
})

test.serial.skip('get nested results as ids', async (t) => {
  const client = connect({ port })

  const matches = []
  const teams = []

  for (let i = 0; i < 100; i++) {
    teams.push({
      $id: await client.id({ type: 'team' }),
      name: 'team ' + i,
      type: 'team',
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
          teams[~~(Math.random() * teams.length)].$id,
        ],
      },
      status: i < 5 ? 100 : 300,
    })
  }

  await Promise.all(teams.map((t) => client.set(t)))

  await client.set({
    type: 'league',
    name: 'league 1',
    children: matches,
  })

  const result = await client.get({
    items: {
      name: true,
      parents: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
          ],
        },
      },
    },
  })

  t.is(result.items.length, 10, 'items length')
  t.is(result.items[0].parents.length, 2, 'has teams')

  await client.delete('root')
  await client.destroy()
})

test.serial('get nested results without find', async (t) => {
  const client = connect({ port })

  const matches = []
  const teams = []

  for (let i = 0; i < 100; i++) {
    teams.push({
      $id: await client.id({ type: 'team' }),
      name: 'team ' + i,
      type: 'team',
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
          teams[~~(Math.random() * teams.length)].$id,
        ],
      },
      status: i < 5 ? 100 : 300,
    })
  }

  await Promise.all(teams.map((t) => client.set(t)))

  await client.set({
    type: 'league',
    name: 'league 1',
    children: matches,
  })

  const result = await client.get({
    name: true,
    id: true,
    children: {
      id: true,
      name: true,
      children: {
        id: true,
        name: true,
        // $list: { $find: { $traverse: 'children' } }
        $list: true,
      },
      $list: true,
    },
  })

  const child = result.children.find((c) => c.children.length)

  t.is(child.children.length, 10, 'has teams')

  await client.delete('root')
  await client.destroy()
})

test.serial('nested refs', async (t) => {
  const client = connect({ port })

  for (let i = 0; i < 3; i++) {
    await client.set({
      type: 'thing',
      docs: [...Array(2)].map((_, i) => ({
        type: 'file',
        name: `file${i}.txt`,
        mirrors: [
          {
            type: 'mirror',
            url: `http://localhost:3000/file${i}.txt`,
          },
          {
            type: 'mirror',
            url: `http://localhost:3001/file${i}.txt`,
          },
        ]
      }))
    })
  }

  const q = {
    thingies: {
      $id: 'root',
      id: true,
      name: true,
      docs: {
        $list: true,
        name: true,
        mirrors: {
          $list: true,
          url: true,
        },
      },
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'thing',
          },
        },
      },
    },
  }
  const res = await client.get(q)
  t.truthy(res.thingies['0'].docs[0].mirrors)

  await client.delete('root')
  await client.destroy()
})
