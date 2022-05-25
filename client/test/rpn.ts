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
  await wait(100)
})

test.after(async (t) => {
  const client = connect({ port })
  //await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('eval boolean expressions', async (t) => {
  const client = connect({ port })

  // test ternary
  const expr1 = '#1 #0 @1 T'
  t.deepEqual(
    await client.redis.selva_rpn_evalbool('node123456', expr1, '0'),
    1
  )
  t.deepEqual(
    await client.redis.selva_rpn_evalbool('node123456', expr1, '1'),
    0
  )

  // Test duplicate
  t.deepEqual(
    await client.redis.selva_rpn_evalbool('node123456', '#1 R A'),
    1
  )

  await client.destroy()
})

test.serial('eval to double', async (t) => {
  const client = connect({ port })

  // Test ternary
  const expr1 = '@2 #1 A @2 #2 A @1 T'
  t.deepEqual(
    Number(await client.redis.selva_rpn_evaldouble('node123456', expr1, '0', '3')),
    4
  )
  t.deepEqual(
    Number(await client.redis.selva_rpn_evaldouble('node123456', expr1, '1', '3')),
    5
  )

  // Test duplicate
  t.deepEqual(
    Number(await client.redis.selva_rpn_evaldouble('node123456', '#1 R A')),
    2
  )

  // Test swap, duplicate, and forward jump
  const expr2 = '@1 R #5 S I >2 #0 D'
  t.deepEqual(
    Number(await client.redis.selva_rpn_evaldouble('node123456', expr2, '3')),
    0
  )
  t.deepEqual(
    Number(await client.redis.selva_rpn_evaldouble('node123456', expr2, '6')),
    6
  )

  // Drop
  t.deepEqual(
    Number(await client.redis.selva_rpn_evaldouble('node123456', '#3 #2 U')),
    3
  )

  // drop, forward jump, and swap
  const expr3 = '@2 @1 R #5 H >1 S U'
  t.deepEqual(
    Number(await client.redis.selva_rpn_evaldouble('node123456', expr3, '3', '6')),
    3
  )
  t.deepEqual(
    Number(await client.redis.selva_rpn_evaldouble('node123456', expr3, '10', '6')),
    6
  )

  // Over
  // a * (a + b)
  t.deepEqual(
    Number(await client.redis.selva_rpn_evaldouble('node123456', '#2 #3 V A D')),
    10
  )

  // rotate
  //   ab - bc
  // = 4 * (10 - 5)
  const expr4 = '@3 @2 @1 W B D'
  t.deepEqual(
    Number(await client.redis.selva_rpn_evaldouble('node123456', expr4, '10', '4', '5')),
    4 * (10 - 5)
  )

  await client.destroy()
})

test.serial('eval to string', async (t) => {
  const client = connect({ port })

  t.deepEqual(
    await client.redis.selva_rpn_evalstring('node123456', '"hello"'),
    'hello'
  )

  const expr1 = '@1 #1 A #2 F L >3 "true" #1 >1 "false"'
  t.deepEqual(
    await client.redis.selva_rpn_evalstring('node123456', expr1, '1'),
    'true'
  )
  t.deepEqual(
    await client.redis.selva_rpn_evalstring('node123456', expr1, '0'),
    'false'
  )

  await client.destroy()
})
