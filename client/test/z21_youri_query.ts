import { join as pathJoin } from 'path'
import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait, removeDump } from './assertions'
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
    languages: ['en', 'de'],
    types: {
      author: {
        fields: {
          books: {
            type: 'references',
            bidirectional: { fromField: 'authors' },
          },
        },
      },
      book: {
        fields: {
          genre: { type: 'string' },
          authors: {
            type: 'references',
            bidirectional: { fromField: 'books' },
          },
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

test.serial.skip('find books', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  // let n = 5
  // while (n--) {
  await client.set({
    type: 'book',
    genre: 'thriller',
  })

  await client.set({
    type: 'author',
  })
  // }

  console.log(
    await client.get({
      $language: 'en',
      books: {
        id: true,
        $list: {
          $find: {
            $traverse: 'descendants',
            $filter: {
              $field: 'type',
              $operator: '!=',
              $value: 'author',
            },
          },
        },
      },
    })
  )

  await client.destroy()
})

// test.serial.skip('find books', async (t) => {
//   const client = connect({ port }, { loglevel: 'info' })

//   const thrillerBook = await client.set({
//     type: 'book',
//     genre: 'thriller'
//   })

//   const authorId = await client.set({
//     type: 'author',
//     books: [thrillerBook]
//   })

//   await client.set({
//     type: 'author',
//     books: []
//   })

//   console.log(await client.get({
//     booksByAuthorX: {
//       $all: true,
//       $list:{
//         $find: {
//           $traverse: 'descendants',
//           $filter: {
//             $field: 'authors',
//             $operator: 'has',
//             $value: authorId
//           }
//         }
//       }
//     }
//   }))

// //   console.log(await client.get({
// //     authorsWithThrillers: {
// //         $all: true,
// //         $list: {
// //             $find: {
// //                 $traverse: 'descendants',
// //                 $filter: [{
// //                     $field: 'type',
// //                     $operator: '=',
// //                     $value: 'book'
// //                 }, {
// //                     $field: 'genre',
// //                     $operator: '=',
// //                     $value: 'thriller'
// //                 }],
// //                 $find: {
// //                   $traverse: 'authors'
// //                 }
// //             }
// //         }
// //     },
// //     authorsWithoutBooks: {
// //       $all: true,
// //       $list: {
// //           $find: {
// //               $traverse: 'descendants',
// //               $filter: [{
// //                   $field: 'type',
// //                   $operator: '=',
// //                   $value: 'author'
// //               }, {
// //                   $field: 'books',
// //                   $operator: 'notExists'
// //               }]
// //           }
// //       }
// //     }
// // }))

//   await client.destroy()
// })
