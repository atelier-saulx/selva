import test from 'ava'
import { start } from '../src/index'
import redis from 'redis'

test.cb('create a server', t => {
  start({ port: 6061 }).then(server => {
    setTimeout(() => {
      const sub = redis.createClient({ port: 6061 })
      const pub = redis.createClient({ port: 6061 })
      sub.subscribe('flap')
      let isClosed = false
      server.on('close', code => {
        isClosed = true
      })
      sub.on('message', async (_, message) => {
        t.is(message, 'smurk')
        await server.destroy()
        t.true(isClosed, 'server is closed')
        const cl = redis.createClient({ port: 6061 })
        cl.on('error', err => {
          t.end()
        })
      })
      pub.publish('flap', 'smurk')
    }, 100)
  })
})
