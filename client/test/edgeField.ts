import test from 'ava'
const { compile, createRecord } = require('data-record')
import { connect } from '../src/index'
import { setRecordDefCstring } from '../src/set/modifyDataRecords'
import { start } from '@saulx/selva-server'
import redis, { RedisClient } from 'redis'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number

const toCArr = (arr) => arr.map(s => s.padEnd(10, '\0')).join('')

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        value: { type: 'number' },
        nested: {
          type: 'object',
          properties: {
            fun: { type: 'string' },
          },
        },
      },
    },
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          value: { type: 'number' },
          description: { type: 'text' },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  await client.destroy()
})

test.after(async (_t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('basic edge ops', async (t) => {
  const client = connect({ port })

  // Create nodes
  await client.redis.selva_modify('root', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma1', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma2', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma3', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma4', '', '0', 'o.a', 'hello')

  // Create edges
  await client.redis.selva_modify('ma2', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma1', 'ma3']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma3', '', '5', 'refs', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma4']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma4', '', '5', 'refs', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma2']),
    $delete: null,
    $value: null,
  }))

  t.deepEqual(
    await client.redis.selva_hierarchy_edgelist('___selva_hierarchy', 'ma1'),
    []
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgelist('___selva_hierarchy', 'ma2'),
    ['a', [
      'b', [
       'ma1',
       'ma3',
      ],
    ]]
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgelist('___selva_hierarchy', 'ma2', 'a'),
    ['b', [
      'ma1',
       'ma3',
      ],
    ]
  )

  await t.throwsAsync(() => client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'ma2', 'a'))

  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'ma2', 'a.b'),
    [ 0, 'ma1', 'ma3' ]
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'ma3', 'refs'),
    [ 0, 'ma4' ]
  )

  // Delete ma3
  await client.redis.selva_hierarchy_del('___selva_hierarchy', 'ma3'),

  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'ma2', 'a.b'),
    [ 0, 'ma1' ]
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'ma4', 'refs'),
    [ 0, 'ma2' ]
  )
})

test.serial('traverse a custom field', async (t) => {
  const client = connect({ port })

  // Create nodes
  await client.redis.selva_modify('root', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma1', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma2', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma3', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma4', '', '0', 'o.a', 'hello')

  // Create edges
  await client.redis.selva_modify('root', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma1', 'ma2']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma1', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma3']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma2', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma4']),
    $delete: null,
    $value: null,
  }))

  t.deepEqual(
    await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'bfs_edge_field', 'a.b', 'root'),
    [ 'root', 'ma1', 'ma2', 'ma3', 'ma4' ]
  )
})

test.serial('find can return edge fields', async (t) => {
  const client = connect({ port })

  // Create nodes
  await client.redis.selva_modify('root', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma1', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma2', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma3', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma4', '', '0', 'o.a', 'hello')

  // Create edges
  await client.redis.selva_modify('root', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma1', 'ma2']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma1', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma3']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma2', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma4']),
    $delete: null,
    $value: null,
  }))

  t.deepEqual(
    await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'bfs_edge_field', 'a.b', 'fields', 'a.b\nparents', 'root'),
    [
      [
        'root', [
          'a.b', ['ma1', 'ma2'],
          'parents', [],
        ]
      ],
      [
        'ma1', [
          'a.b', ['ma3'],
          'parents', ['root'],
        ],
      ],
      [
        'ma2', [
          'a.b', ['ma4'],
          'parents', ['root'],
        ],
      ],
      [
        'ma3', [
          'parents', ['root'],
        ]
      ],
      [
        'ma4', [
          'parents', ['root'],
        ],
      ],
    ]
  )
})

test.serial('find can do nested traversals', async (t) => {
  const client = connect({ port })

  // Create nodes
  await client.redis.selva_modify('root', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma01', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma02', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma11', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma12', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma21', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma22', '', '0', 'o.a', 'hello')

  // Create edges
  await client.redis.selva_modify('root', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma01', 'ma02']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma01', '', '5', 'children', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma11']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma02', '', '5', 'children', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma12']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma11', '', '5', 'children', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma21']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma12', '', '5', 'children', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma22']),
    $delete: null,
    $value: null,
  }))

  t.deepEqual(
    await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'bfs_edge_field', 'a.b', 'fields', 'a.b\nparents\ndescendants', 'root'),
    [
      [
        'root', [
          'a.b', ['ma01', 'ma02'],
          'parents', [],
          'descendants', [
             'ma01',
             'ma02',
             'ma11',
             'ma12',
             'ma21',
             'ma22',
          ],
        ]
      ],
      [
        'ma01', [
          'parents', ['root'],
          'descendants', ['ma11', 'ma21'],
        ],
      ],
      [
        'ma02', [
          'parents', ['root'],
          'descendants', ['ma12', 'ma22'],
        ],
      ],
    ]
  )
})

test.serial('missing edges are added automatically', async (t) => {
  const client = connect({ port })

  // Create nodes
  t.deepEqual(
    await client.redis.selva_modify('root', '', '0', 'o.a', 'hello'),
    ['root', 'UPDATED']
  )

  await client.redis.selva_modify('root', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma1', 'ma2']),
    $delete: null,
    $value: null,
  }))

  t.deepEqual(
    await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'bfs_edge_field', 'a.b', 'root'),
    [ 'root', 'ma1', 'ma2' ]
  )
})

test.serial('edge modify `add` values diff', async (t) => {
  const client = connect({ port })

  // Create nodes
  t.deepEqual(
    await client.redis.selva_modify('root', '', '0', 'o.a', 'hello'),
    ['root', 'UPDATED']
  )

  const rec = createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma1', 'ma2']),
    $delete: null,
    $value: null,
  })

  t.deepEqual(
    await client.redis.selva_modify('root', '', '5', 'a.b', rec),
    ['root', 'UPDATED']
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'root', 'a.b'),
    [0, 'ma1', 'ma2']
  )
  t.deepEqual(
    await client.redis.selva_modify('root', '', '5', 'a.b', rec),
    ['root', 'OK']
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'root', 'a.b'),
    [0, 'ma1', 'ma2']
  )
})

test.serial('edge modify `delete` values diff', async (t) => {
  const client = connect({ port })

  // Create nodes
  t.deepEqual(
    await client.redis.selva_modify('root', '', '0', 'o.a', 'hello'),
    ['root', 'UPDATED']
  )

  const rec1 = createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma1', 'ma2']),
    $delete: null,
    $value: null,
  })
  const rec2 = createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: null,
    $delete: toCArr(['ma1', 'ma2']),
    $value: null,
  })

  t.deepEqual(
    await client.redis.selva_modify('root', '', '5', 'a.b', rec1),
    ['root', 'UPDATED']
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'root', 'a.b'),
    [0, 'ma1', 'ma2']
  )
  t.deepEqual(
    await client.redis.selva_modify('root', '', '5', 'a.b', rec2),
    ['root', 'UPDATED']
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'root', 'a.b'),
    [0]
  )
  t.deepEqual(
    await client.redis.selva_modify('root', '', '5', 'a.b', rec2),
    ['root', 'OK']
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'root', 'a.b'),
    [0]
  )
})

test.serial('edge modify `value` values diff', async (t) => {
  const client = connect({ port })

  // Create nodes
  t.deepEqual(
    await client.redis.selva_modify('root', '', '0', 'o.a', 'hello'),
    ['root', 'UPDATED']
  )

  const rec = createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: null,
    $delete: null,
    $value: toCArr(['ma1', 'ma2']),
  })

  t.deepEqual(
    await client.redis.selva_modify('root', '', '5', 'a.b', rec),
    ['root', 'UPDATED']
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'root', 'a.b'),
    [0, 'ma1', 'ma2']
  )
  t.deepEqual(
    await client.redis.selva_modify('root', '', '5', 'a.b', rec),
    ['root', 'OK']
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'root', 'a.b'),
    [0, 'ma1', 'ma2']
  )
})

test.serial('edge modify `add` and `delete` values diff', async (t) => {
  const client = connect({ port })

  // Create nodes
  t.deepEqual(
    await client.redis.selva_modify('root', '', '0', 'o.a', 'hello'),
    ['root', 'UPDATED']
  )

  const rec1 = createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: null,
    $delete: null,
    $value: toCArr(['ma1']),
  })
  const rec2 = createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma2', 'ma3']),
    $delete: toCArr(['ma1']),
    $value: null,
  })

  t.deepEqual(
    await client.redis.selva_modify('root', '', '5', 'a.b', rec1),
    ['root', 'UPDATED']
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'root', 'a.b'),
    [0, 'ma1']
  )
  t.deepEqual(
    await client.redis.selva_modify('root', '', '5', 'a.b', rec2),
    ['root', 'UPDATED']
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'root', 'a.b'),
    [0, 'ma2', 'ma3']
  )
  t.deepEqual(
    await client.redis.selva_modify('root', '', '5', 'a.b', rec2),
    ['root', 'OK']
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'root', 'a.b'),
    [0, 'ma2', 'ma3']
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_del('___selva_hierarchy', 'ma1'),
    1
  )
})

test.serial('edge modify `delete_all`', async (t) => {
  const client = connect({ port })

  // Create nodes
  t.deepEqual(
    await client.redis.selva_modify('root', '', '0', 'o.a', 'hello'),
    ['root', 'UPDATED']
  )

  const rec1 = createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma1', 'ma2']),
    $delete: null,
    $value: null,
  })
  const rec2 = createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 1,
    constraint_id: 0,
    $add: null,
    $delete: null,
    $value: null,
  })

  t.deepEqual(
    await client.redis.selva_modify('root', '', '5', 'a.b', rec1),
    ['root', 'UPDATED']
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'root', 'a.b'),
    [0, 'ma1', 'ma2']
  )
  t.deepEqual(
    await client.redis.selva_modify('root', '', '5', 'a.b', rec2),
    ['root', 'UPDATED']
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'root', 'a.b'),
    [0]
  )
})

test.serial('traverse by expression', async(t) => {
  const client = connect({ port })

  // Create nodes
  t.deepEqual(
    await client.redis.selva_modify('root', '', '0', 'o.a', 'hello'),
    ['root', 'UPDATED']
  )

  const rec1 = createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma1']),
    $delete: null,
    $value: null,
  })
  const rec2 = createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma2']),
    $delete: null,
    $value: null,
  })

  t.deepEqual(
    await client.redis.selva_modify('root', '', '5', 'a', rec1),
    ['root', 'UPDATED']
  )
  t.deepEqual(
    await client.redis.selva_modify('root', '', '5', 'b', rec2),
    ['root', 'UPDATED']
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'bfs_expression', '{"a","b"}', 'root'),
    [ 'ma1', 'ma2' ]
  )
})

test.serial('deref node references on find', async(t) => {
  const client = connect({ port })

  // Create nodes
  t.deepEqual(
    await client.redis.selva_modify('match1', '', '0', 'title', 'Best Game'),
    ['match1', 'UPDATED']
  )
  t.deepEqual(
    await client.redis.selva_modify('team1', '', '0', 'name', 'Funny Team'),
    ['team1', 'UPDATED']
  )
  t.deepEqual(
    await client.redis.selva_modify('club1', '', '0', 'name', 'Funny Club'),
    ['club1', 'UPDATED']
  )
  t.deepEqual(
    await client.redis.selva_modify('manager1', '', '0', 'name', 'dung'),
    ['manager1', 'UPDATED']
  )

  const rec1 = createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 1,
    $add: toCArr(['team1']),
    $delete: null,
    $value: null,
  })
  const rec2 = createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 1,
    $add: toCArr(['club1']),
    $delete: null,
    $value: null,
  })
  const rec3 = createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 1,
    $add: toCArr(['manager1']),
    $delete: null,
    $value: null,
  })

  t.deepEqual(
    await client.redis.selva_modify('match1', '', '5', 'homeTeam', rec1),
    ['match1', 'UPDATED']
  )
  t.deepEqual(
    await client.redis.selva_modify('team1', '', '5', 'club', rec2),
    ['team1', 'UPDATED']
  )
  t.deepEqual(
    await client.redis.selva_modify('club1', '', '5', 'manager', rec3),
    ['club1', 'UPDATED']
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'node', 'fields', 'title\nhomeTeam.name\nhomeTeam.club.name\nhomeTeam.club.manager.name', 'match1'),
    [[ 'match1', [ 'title', 'Best Game', 'homeTeam.name', 'Funny Team', 'homeTeam.club.name', 'Funny Club', 'homeTeam.club.manager.name', 'dung' ]]]
  )
})

test.serial('bidirectional edge fields', async (t) => {
  const client = connect({ port })

  // Create dynamic constraints
  await client.redis.selva_hierarchy_addconstraint('___selva_hierarchy',
      'te', // source node type
      'players', // source field name
      '2', // constraint flags
      'team' // bck field name
  )
  await client.redis.selva_hierarchy_addconstraint('___selva_hierarchy',
      'pl',
      'team',
      '3',
      'players'
  )

  // Create nodes
  await client.redis.selva_modify('root', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('te1', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('pl1', '', '0', 'o.a', 'tim')
  await client.redis.selva_modify('pl2', '', '0', 'o.a', 'bob')
  await client.redis.selva_modify('pl3', '', '0', 'o.a', 'jack')

  // Create edges
  await client.redis.selva_modify('root', '', '5', 'teams', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['te1']),
    $delete: null,
    $value: null,
  }))
  t.deepEqual(await client.redis.selva_modify('te1', '', '5', 'players', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 2,
    $add: null,
    $delete: null,
    $value: toCArr(['pl1', 'pl2', 'pl3']),
  })), ['te1', 'UPDATED'])

  t.deepEqual(
    await client.redis.selva_hierarchy_edgelist('___selva_hierarchy', 'te1'),
    ['players', [
        'pl1',
        'pl2',
        'pl3',
      ],
    ]
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgelist('___selva_hierarchy', 'pl1'),
    ['team', [
        'te1',
      ]
    ]
  )

  // Delete an edge
  t.deepEqual(await client.redis.selva_modify('pl3', '', '5', 'team', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 1,
    constraint_id: 2,
    $add: null,
    $delete: null,
    $value: null,
  })), ['pl3', 'UPDATED'])
  t.deepEqual(
    await client.redis.selva_hierarchy_edgelist('___selva_hierarchy', 'te1'),
    ['players', [
        'pl1',
        'pl2',
      ],
    ]
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgelist('___selva_hierarchy', 'pl3'),
    ['team', [
      ]
    ]
  )

  // Delete an edge
  t.deepEqual(await client.redis.selva_modify('pl2', '', '5', 'team', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 2,
    $add: null,
    $delete: toCArr(['te1']),
    $value: null,
  })), ['pl2', 'UPDATED'])
  t.deepEqual(
    await client.redis.selva_hierarchy_edgelist('___selva_hierarchy', 'te1'),
    ['players', [
        'pl1',
      ],
    ]
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgelist('___selva_hierarchy', 'pl2'),
    ['team', [
      ]
    ]
  )
})
