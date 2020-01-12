// import test from 'ava'
// import { connect, SelvaClient } from '../src/index'
// import { start } from 'selva-server'

// test.before(async t => {
//   await start({ port: 6061, modules: ['redisearch'] })
// })

// test.serial('get - basic', async t => {
//   const client = connect({ port: 6061 })

//   await client.set({
//     $id: 'viA',
//     title: {
//       en: 'nice!'
//     },
//     value: 25,
//     auth: {
//       // role needs to be different , different roles per scope should be possible
//       role: {
//         id: ['root'],
//         type: 'admin'
//       }
//     }
//   })

//   t.deepEqual(
//     await client.get({
//       $id: 'viA',
//       id: true,
//       title: true,
//       value: true
//     }),
//     {
//       id: 'viA',
//       title: { en: 'nice!' },
//       value: 25
//     }
//   )

//   t.deepEqual(
//     await client.get({
//       $id: 'viA',
//       auth: true
//     }),
//     {
//       auth: { role: { id: ['root'], type: 'admin' } }
//     }
//   )

//   t.deepEqual(
//     await client.get({
//       $id: 'viA',
//       auth: { role: { id: true } }
//     }),
//     {
//       auth: { role: { id: ['root'] } }
//     }
//   )

//   await client.delete('root')

//   client.destroy()
// })

// test.serial('get - $default', async t => {
//   const client = connect({ port: 6061 })

//   await client.set({
//     $id: 'viflap',
//     title: { en: 'flap' }
//   })

//   t.deepEqual(
//     await client.get({
//       $id: 'viflap',
//       age: { $default: 100 }
//     }),
//     { age: 100 }
//   )

//   t.deepEqual(
//     await client.get({
//       $id: 'viflap',
//       title: {
//         en: { $default: 'untitled' },
//         nl: { $default: 'naamloos' }
//       }
//     }),
//     {
//       title: { en: 'flap', nl: 'naamloos' }
//     }
//   )

//   await client.delete('root')

//   client.destroy()
// })

// // also need a get $all (when just passing id perhaps?) or $all: true
// // also need an exists (when just passing id?)
// test.serial('get - $language', async t => {
//   const client = connect({ port: 6061 })

//   await client.set({
//     $id: 'viflap',
//     title: { en: 'flap', nl: 'flurp' },
//     description: { en: 'yes', nl: 'ja' }
//   })

//   t.deepEqual(
//     await client.get({
//       $id: 'viflap',
//       title: true,
//       description: true,
//       $language: 'nl'
//     }),
//     {
//       // thinkg about this
//       title: 'flurp',
//       description: 'ja'
//     }
//   )

//   await client.delete('root')

//   client.destroy()
// })
