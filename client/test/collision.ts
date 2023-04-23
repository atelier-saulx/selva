import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import getPort from 'get-port'
import { wait } from '@saulx/utils'
let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })

  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de'],
    types: {
      thing: {
        fields: {
          name: { type: 'string' },
        },
      },
    },
  })
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial.only('collision', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  let n = 100
  const allIds: any = []
  while (n--) {
    const ids = await Promise.all(
      Array.from(Array(5000)).map(() => {
        return client.set({
          type: 'thing',
        })
      })
    )

    allIds.push(...ids)
    const expectedUniqueIds = allIds.length
    const actualUniqueIds = new Set(allIds).size
    if (expectedUniqueIds !== actualUniqueIds) {
      console.log({ expectedUniqueIds, actualUniqueIds })
      t.fail()
      break
    }
  }

  t.pass()

  await client.destroy()
})
