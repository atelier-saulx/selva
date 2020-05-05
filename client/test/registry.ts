import test from 'ava'
import { connect } from '../src/index'
import { startRegistry } from '@saulx/selva-server'
// import './assertions'
// import { wait } from './assertions'

// let srv

startRegistry({}).then(server => {
  const x = connect({ port: server.port })

  console.log('start client')
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
