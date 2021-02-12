import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
// @ts-ignore suppressing module can only be default-imported using the 'esModuleInterop' flag
import getPort from 'get-port'

let srv
let port

test.before(async (t) => {
  port = await getPort()
  srv = await start({ port })
  await wait(500)
  const client = connect({ port: port })
  await client.updateSchema({
    types: {
      genre: {
        prefix: 'ge',
        fields: {
          name: { type: 'string' },
          icon: { type: 'string' },
          fields: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              something: { type: 'string' },
            },
          },
          theme: {
            type: 'object',
            properties: {
              colors: {
                type: 'object',
                properties: {
                  color1: { type: 'string' },
                  color2: { type: 'string' },
                },
              },
            },
          },
        },
      },
      movie: {
        prefix: 'mo',
        fields: {
          title: { type: 'string' },
        },
      },
      imaginary: {
        prefix: 'im',
        fields: {
          imaginary: { type: 'string' },
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

test.serial('simple', async (t) => {
  const client = connect({ port: port })

  const genre = await client.set({
    $id: 'geScifi',
    name: 'Sci-fi',
    icon: 'scifi.png',
  })

  await client.set({
    $id: 'moSoylentG',
    title: 'Soylent Green',
    parents: [genre],
  })

  const result = await client.get({
    $id: 'moSoylentG',
    icon: { $inherit: true },
  })

  t.true(result.icon === 'scifi.png')

  await client.destroy()
})

test.serial('simple with circular', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  const genre = await client.set({
    $id: 'geScifi',
    name: 'Sci-fi',
    icon: 'scifi.png',
    parents: ['moSoylentGreen'],
  })

  await client.set({
    $id: 'moSoylentG',
    title: 'Soylent Green',
    parents: [genre],
  })

  const result = await client.get({
    $id: 'moSoylentG',
    icon: { $inherit: true },
    imaginary: { $inherit: true },
  })

  t.true(result.icon === 'scifi.png')

  await client.destroy()
})

test.serial('$all', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  const genre = await client.set({
    $id: 'geA',
    fields: {
      name: 'hello',
    },
  })

  const genre2 = await client.set({
    $id: 'geB',
    parents: ['geA'],
  })

  const result1 = await client.get({
    $id: 'geA',
    fields: {
      $all: true,
      $inherit: { $type: 'genre' },
    },
  })

  t.deepEqual(result1.fields, { name: 'hello' }, 'from field')

  const result = await client.get({
    $id: 'geB',
    fields: {
      $all: true,
      $inherit: { $type: 'genre' },
    },
  })

  t.deepEqual(result.fields, { name: 'hello' }, 'inherit')

  await client.destroy()
})

test.serial('$field + object', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  const genre = await client.set({
    $id: 'geC',
    fields: {
      name: 'hello',
    },
  })

  const genre2 = await client.set({
    $id: 'geD',
    parents: ['geC'],
  })

  const result1 = await client.get({
    $id: 'geC',
    flaprdol: {
      name: { $field: 'fields.name' },
      $inherit: { $type: 'genre' },
    },
  })

  t.deepEqual(result1, { flaprdol: { name: 'hello' } }, 'get')

  const result = await client.get({
    $id: 'geD',
    flaprdol: {
      name: { $field: 'fields.name' },
      $inherit: { $type: 'genre' },
    },
  })

  t.deepEqual(result, { flaprdol: { name: 'hello' } }, 'inherit')

  await client.destroy()
})

test.serial('$field + object + all', async (t) => {
  const client = connect({ port: port })

  const genre = await client.set({
    $id: 'geC',
    fields: {
      name: 'hello',
    },
  })

  const genre2 = await client.set({
    $id: 'geD',
    parents: ['geC'],
  })

  const result1 = await client.get({
    $id: 'geC',
    flaprdol: {
      $all: true,
      $field: 'fields',
      $inherit: { $type: ['genre'] },
    },
  })

  t.deepEqual(result1, { flaprdol: { name: 'hello' } }, 'get')

  const result = await client.get({
    $id: 'geD',
    flaprdol: {
      $all: true,
      $field: 'fields',
      $inherit: { $type: 'genre' },
    },
  })

  t.deepEqual(result, { flaprdol: { name: 'hello' } }, 'inherit')

  await client.destroy()
})

test.serial('$field + object + all + nested', async (t) => {
  const client = connect({ port: port })

  const genre = await client.set({
    $id: 'geX',
    theme: {
      colors: {
        color1: '1',
        color2: '2',
      },
    },
  })

  const genre2 = await client.set({
    $id: 'geY',
    parents: ['geX'],
  })

  const result1 = await client.get({
    $id: 'geX',
    myTheme: {
      $field: 'theme',
      $inherit: { $type: 'genre' },
      $all: true,
    },
  })

  t.deepEqual(
    result1,
    {
      myTheme: {
        colors: {
          color1: '1',
          color2: '2',
        },
      },
    },
    'get'
  )

  const result = await client.get({
    $id: 'geY',
    myTheme: {
      $field: 'theme',
      $inherit: { $type: 'genre' },
      $all: true,
    },
  })

  t.deepEqual(
    result1,
    {
      myTheme: {
        colors: {
          color1: '1',
          color2: '2',
        },
      },
    },
    'inherit'
  )

  await client.destroy()
})

test.serial('$field +  multiple options', async (t) => {
  const client = connect({ port: port })

  const types = ['match', 'region']

  const layout = {
    type: 'object',
    properties: [...types, 'default'].reduce((properties, type) => {
      properties[type] = {
        type: 'json',
      }
      return properties
    }, {}),
  }

  await client.updateSchema({
    types: {
      match: {
        prefix: 'ma',
        fields: {
          //@ts-ignore
          layout,
        },
      },
      region: {
        prefix: 're',
        fields: {
          //@ts-ignore
          layout,
        },
      },
    },
  })

  await client.set({
    $id: 'reA',
    layout: {
      default: {
        components: {
          c1: {
            component: { $value: 'bye' },
          },
        },
      },
      region: {
        components: {
          c1: {
            component: { $value: 'hello' },
          },
        },
      },
    },
  })

  await client.set({
    $id: 'maA',
    parents: ['reA'],
  })

  await client.set({
    $id: 'reB',
    parents: ['reA'],
  })

  const g = await client.get({
    $id: 'reA',
    $all: true,
  })

  t.is(g.layout.default.components.c1.component.$value, 'bye')

  const query = {
    $alias: 'maA',
    id: true,
    layout: {
      // $inherit: { $type: ['match', 'region', 'root'] },
      $inherit: true,
      $field: ['layout.match', 'layout.default'],
    },
  }

  const x = await client.get(query)

  t.is(x.layout.components.c1.component.$value, 'bye')

  const y = await client.get({
    $alias: 'reB',
    id: true,
    layout: {
      // $inherit: { $type: ['match', 'region', 'root'] },
      $inherit: true,
      $field: ['layout.region', 'layout.default'],
    },
  })

  t.is(y.layout.components.c1.component.$value, 'hello')

  t.true(true)

  await client.destroy()
})

test.serial('$field +  multiple options + inherit from root', async (t) => {
  const client = connect({ port: port })

  // adding extra field to schema as well
  const types = ['match', 'region', 'root', 'default']
  // is failing

  const layout = {
    type: 'object',
    properties: types.reduce((properties, type) => {
      properties[type] = {
        type: 'json',
      }
      return properties
    }, {}),
  }

  try {
    await client.updateSchema({
      rootType: {
        fields: {
          //@ts-ignore
          layout,
        },
      },
      types: {
        match: {
          prefix: 'ma',
          fields: {
            //@ts-ignore
            layout,
          },
        },
        region: {
          prefix: 're',
          fields: {
            //@ts-ignore
            layout,
          },
        },
      },
    })
  } catch (err) {
    t.fail('should be able to update layout fields root ' + err.message)
  }

  await client.set({
    $id: 'root',
    layout: {
      default: {
        components: {
          c1: {
            component: { $value: 'bye' },
          },
        },
      },
      region: {
        components: {
          c1: {
            component: { $value: 'hello' },
          },
        },
      },
    },
  })

  await client.set({
    $id: 'maA',
    parents: ['root'],
  })

  await client.set({
    $id: 'reB',
    parents: ['root'],
  })

  const g = await client.get({
    $id: 'root',
    $all: true,
  })

  t.is(g.layout.default.components.c1.component.$value, 'bye')

  const query = {
    $alias: 'maA',
    id: true,
    layout: {
      $inherit: { $type: ['match', 'region', 'root'] },
      $field: ['layout.match', 'layout.default'],
    },
  }

  const x = await client.get(query)

  t.is(x.layout.components.c1.component.$value, 'bye')

  const y = await client.get({
    $alias: 'reB',
    id: true,
    layout: {
      $inherit: { $type: ['match', 'region', 'root'] },
      $field: ['layout.region', 'layout.default'],
    },
  })

  t.is(y.layout.components.c1.component.$value, 'hello')

  t.true(true)

  await client.destroy()
})

test.serial('$field + inherit from root + query root', async (t) => {
  const p = await getPort()
  const srv = await start({ port: p })

  const client = connect({ port: p }, { loglevel: 'info' })
  const types = ['match', 'region', 'root', 'default']

  const layout = {
    type: 'object',
    meta: { type: 'layout', useTabs: true },
    properties: types.reduce((properties, type) => {
      properties[type] = {
        type: 'object',
        properties: {
          components: {
            meta: { expand: true },
            type: 'array',
            items: {
              meta: { type: 'query' },
              type: 'json',
            },
          },
          userComponents: {
            meta: { expand: true },
            type: 'array',
            items: {
              meta: { type: 'query' },
              type: 'json',
            },
          },
        },
      }
      return properties
    }, {}),
  }

  try {
    await client.updateSchema({
      rootType: {
        fields: {
          //@ts-ignore
          layout,
        },
      },
      types: {
        match: {
          prefix: 'ma',
          fields: {
            //@ts-ignore
            layout,
          },
        },
        region: {
          prefix: 're',
          fields: {
            //@ts-ignore
            layout,
          },
        },
      },
    })
  } catch (err) {
    t.fail('should be able to update layout fields root ' + err.message)
  }

  await client.set({
    $id: 'root',
    layout: {
      default: {
        components: [
          {
            component: { $value: 'bye' },
          },
        ],
        userComponents: [],
      },
    },
  })

  const x = await client.get({
    $id: 'root',
    id: true,
    layout: {
      // $inherit: { $type: ['match', 'region', 'root'] },
      $inherit: true,
      $field: ['layout.match', 'layout.default'],
    },
  })

  t.is(x.layout.components[0].component.$value, 'bye')

  await client.destroy()

  await srv.destroy()
})

test.serial('$all + object', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  const genre = await client.set({
    $id: 'geC',
    fields: {
      name: 'hello',
    },
  })

  const genre2 = await client.set({
    $id: 'geD',
    parents: ['geC'],
  })

  const result1 = await client.get({
    $id: 'geC',
    flaprdol: {
      name: { $field: 'fields.name' },
      $inherit: { $type: 'genre' },
    },
  })

  t.deepEqual(result1, { flaprdol: { name: 'hello' } }, 'get')

  const result = await client.get({
    $id: 'geD',
    $all: true,
    flaprdol: {
      name: { $field: 'fields.name' },
      $inherit: { $type: 'genre' },
    },
  })

  t.deepEqual(
    result,
    { id: 'geD', type: 'genre', flaprdol: { name: 'hello' } },
    'inherit'
  )

  await client.destroy()
})
