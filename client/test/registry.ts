import test from 'ava'
import { SelvaClient } from '../src/index'
import { startRegistry } from '@saulx/selva-server'
// import './assertions'
// import { wait } from './assertions'

// let srv

const x = new SelvaClient({})

console.log(x)

startRegistry({}).then(server => {
  console.log(server.port)
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
