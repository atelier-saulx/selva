import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import getPort from 'get-port'
import { wait } from './assertions'

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
      author: {
        fields: {
          name: { type: 'string' },
        },
      },
      book: {
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
  await t.connectionsAreEmpty()
})

test.serial.only('find things', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  const books = 1000
  const authors = 10

  await Promise.all(
    Array.from(Array(authors)).map(async (_, index) => {
      const authorId = await client.set({
        type: 'author',
        name: 'author ' + index,
      })
      return Promise.all(
        Array.from(Array(books / authors)).map((_, index) => {
          return client.set({
            type: 'book',
            name: 'book ' + index,
            parents: [authorId],
          })
        })
      )
    })
  )

  const query = {
    books: {
      name: true,
      author: {
        id: true,
        name: true,
        $find: {
          $traverse: 'parents',
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'author',
          },
        },
      },
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'book',
          },
        },
      },
    },
  }

  const startGet = Date.now()
  await client.get(query)
  const timeToGet = Date.now() - startGet
  const startSub = Date.now()
  await new Promise((resolve) => client.observe(query).subscribe(resolve))
  const timeToSub = Date.now() - startSub

  t.true(
    timeToSub <= timeToGet + 1e3,
    'time to subscribe is similar to time to get'
  )

  await client.destroy()
})
