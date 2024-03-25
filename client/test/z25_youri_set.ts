import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
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
      ints: {
        prefix: 'in',
        fields: {
          name: { type: 'string' },
          a: { type: 'timestamp' },
          b: { type: 'timestamp' },
          // c: { type: 'timestamp' },
          // d: { type: 'timestamp' },
          // ref: { type: 'reference' },
          rec: {
            type: 'record',
            values: {
              type: 'json',
            },
          },
        },
      },
      thing: {
        fields: {
          mySet: { type: 'set', items: { type: 'timestamp' } },
          mySetObject: {
            type: 'object',
            properties: {
              mySet2: { type: 'set', items: { type: 'timestamp' } },
            },
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
})

test.serial.skip('use set with numbers in object', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const id = await client.set({
    type: 'thing',
    mySetObject: {
      mySet2: {
        $add: Date.now(),
      },
    },
  })

  console.log('HERE:', await client.get({ $id: id, $all: true }))

  t.pass()

  await client.destroy()
})

test.serial.only('int conversion', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  const tsLength = String(Date.now()).length
  let i = 20

  await client.set({
    $id: 'in1',
    a: Date.now(), //1709901791582,
    b: Date.now(),
    // c: Date.now(),
    // d: Date.now(),
    // ref: await client.set({
    //   $id: 'in2',
    //   // a: Date.now(),
    //   // b: Date.now(),
    //   // c: Date.now(),
    //   // d: Date.now(),
    // }),
  })

  // const a = await client
  //   .observe({
  //     $id: 'in1',

  //     $all: true,
  //     // a: true,
  //     // b: true,
  //     // rec: true,
  //     // createdAt: true,
  //     // updatedAt: true,
  //   })
  //   .subscribe(async (res) => {
  //     if (
  //       String(res.a).length !== tsLength ||
  //       String(res.b).length !== tsLength
  //     ) {
  //       console.log(
  //         await Promise.all([
  //           res,
  //           client.get({
  //             $id: 'in1',
  //             $all: true,
  //           }),
  //           client.get({
  //             $id: 'in1',
  //             a: true,
  //             b: true,
  //           }),
  //         ])
  //       )
  //       t.fail()
  //     }
  //   })
  const fails = [1711367107799, 1711367120835]
  while (i--) {
    await new Promise((resolve) => setTimeout(resolve, 200))
    const a = Date.now()
    console.log('+++++++++++++SET:', a)
    await client.set({
      $id: 'in1',
      a,
      b: a,
    })
    const res = await client.get({
      $id: 'in1',
      // a: true,
      // b: true,
      $all: true,
      aliases: false,
    })

    if (
      String(res.a).length !== tsLength ||
      String(res.b).length !== tsLength
    ) {
      console.log('---------------------FAIL---------------------', a) // 1711367107799
      // console.log(
      //   await Promise.all([
      //     res,
      //     client.get({
      //       $id: 'in1',
      //       $all: true,
      //     }),
      //     client.get({
      //       $id: 'in1',
      //       a: true,
      //       b: true,
      //     }),
      //   ])
      // )
      t.fail()
      break
    }
  }

  t.pass()

  await client.destroy()
})
