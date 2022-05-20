import { join as pathJoin } from 'path'
import test from 'ava'
import { connect } from '../src/index'
import { start, startOrigin } from '@saulx/selva-server'
import { removeDump, wait } from './assertions'
import getPort from 'get-port'
import { join } from 'path'

const dbName = 'testit'
const dir = pathJoin(process.cwd(), 'tmp', 'dump-test')

test.before(removeDump(dir))
test.afterEach.always(removeDump(dir))

const testDataType = async (t, schema, payload) => {
  let n = 2

  t.plan(n)

  while (n--) {
    const port = await getPort()
    const registry = await start({
      port,
    })
    const server = await startOrigin({
      name: dbName,
      save: true,
      dir,
      registry: { port },
    })
    const client = connect({ port }, { loglevel: 'info' })

    await client.updateSchema(schema, dbName)
    await client.set(payload)
    await Promise.all([registry.destroy(), server.destroy(), client.destroy()])
    await wait(1e3)

    t.pass()
  }
}

// works
test.serial('works with number: 1', async (t) => {
  await testDataType(
    t,
    {
      types: {
        sport: {
          prefix: 'sp',
          fields: {
            number: { type: 'number' },
          },
        },
      },
    },
    {
      $db: dbName,
      $id: 'sp1',
      number: 1,
    }
  )
})

// breaks
test.serial('works with number: 0', async (t) => {
  await testDataType(
    t,
    {
      types: {
        sport: {
          prefix: 'sp',
          fields: {
            number: { type: 'number' },
          },
        },
      },
    },
    {
      $db: dbName,
      $id: 'sp0',
      number: 0,
    }
  )
})

// works
test.serial('works with boolean: true', async (t) => {
  await testDataType(
    t,
    {
      types: {
        sport: {
          prefix: 'sp',
          fields: {
            bool: { type: 'boolean' },
          },
        },
      },
    },
    {
      $db: dbName,
      $id: 'spTrue',
      bool: true,
    }
  )
})

// breaks
test.serial('works with boolean: false', async (t) => {
  await testDataType(
    t,
    {
      types: {
        sport: {
          prefix: 'sp',
          fields: {
            bool: { type: 'boolean' },
          },
        },
      },
    },
    {
      $db: dbName,
      $id: 'spFalse',
      bool: false,
    }
  )
})

// works
test.serial('works with string: empty', async (t) => {
  await testDataType(
    t,
    {
      types: {
        sport: {
          prefix: 'sp',
          fields: {
            str: { type: 'string' },
          },
        },
      },
    },
    {
      $db: dbName,
      $id: 'spEmpty',
      str: '',
    }
  )
})
