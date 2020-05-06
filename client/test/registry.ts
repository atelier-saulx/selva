import test from 'ava'
import { connect } from '../src/index'
import { startRegistry, startOrigin } from '@saulx/selva-server'
// import './assertions'
// import { wait } from './assertions'

// let srv

startRegistry({}).then(registry => {
  startOrigin({ default: true, registry: { port: registry.port } }).then(
    origin => {
      const x = connect({ port: registry.port })
      console.log('start origin also started dat registry')
    }
  )
})

// connect

// simple redis functions e.g. hget etc

// origin
// cache
// replica
// registry

// subscribe handler
// make subs a bit cleaner

// redis  manages a quue for dc
// redis-client manages buffer and reconn queues

// redis-client allways has a subscriber and publisher client

// if connecting to cache (used for subscriptions) handle things alittle bit different in 'redis'
