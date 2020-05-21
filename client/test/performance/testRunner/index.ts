import { connect, SelvaClient } from '../../../src/index'
import chalk from 'chalk'
import {
  startRegistry,
  startOrigin,
  startSubscriptionManager,
  startReplica,
  SelvaServer
} from '@saulx/selva-server'
import getPort from 'get-port'
import { Worker } from 'worker_threads'
import path from 'path'

let registry
let origin
// let origin2
let replicasList = []
let subsManagersList = []

let replicaAmount = 5
let subManagerAmount = 5

export async function start({ replicas = 5, subsManagers = 5 } = {}): Promise<{
  registry: SelvaServer
}> {
  replicaAmount = replicas
  subsManagers = subsManagers
  const port = await getPort()
  // small test
  registry = await startRegistry({
    port
  })

  origin = startOrigin({ registry: { port } })
  // origin2 = startOrigin({ name: 'origin2', registry: { port } })

  for (let i = 0; i < replicaAmount; i++) {
    replicasList.push(startReplica({ registry: { port } }))
  }

  for (let i = 0; i < subManagerAmount; i++) {
    subsManagersList.push(startSubscriptionManager({ registry: { port } }))
  }

  return { registry }
}

export async function stop() {
  await registry.destroy()
  await origin.destroy()
  for (let i = 0; i < replicaAmount; i++) {
    await replicasList[i].destroy()
  }
  for (let i = 0; i < subManagerAmount; i++) {
    await subsManagersList[i].destroy()
  }
}

const startClient = (
  fn: (client: SelvaClient) => void,
  opts: { time: number; clients: number }
): Promise<void> => {
  return new Promise(r => {
    const worker = (this.worker = new Worker(
      path.join(__dirname, '/clientWorker.js')
    ))
    worker.postMessage(
      JSON.stringify({ event: 'start', fn: fn.toString(), time: opts.time })
    )
    worker.on('message', message => {
      try {
        const obj = JSON.parse(message)
        if (obj.event === 'complete') {
          console.log(obj)
          r(obj)
        }
      } catch (_err) {}
    })
  })
}

export async function run(
  fn: (client: SelvaClient) => void,
  opts: { time: number; clients: number }
): Promise<number> {
  console.log('lets run it!')
  const q = []
  for (let i = 0; i < opts.clients; i++) {
    q.push(startClient(fn, opts))
  }
  const results = await Promise.all(q)
  results.forEach((v, i) => {
    console.log('client', i, v.time.length, 'iterations')
  })
  return 10
}
