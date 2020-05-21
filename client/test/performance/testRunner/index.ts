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

let registry: SelvaServer
let origin: SelvaServer
// let origin2
let replicasList: SelvaServer[] = []
let subsManagersList: SelvaServer[] = []

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

  origin = await startOrigin({ registry: { port }, default: true })

  const client = connect({ port })

  client.updateSchema({
    rootType: {
      fields: { value: { type: 'number' } }
    },
    types: {
      thing: {
        fields: { value: { type: 'number' } }
      }
    }
  })

  // origin2 = startOrigin({ name: 'origin2', registry: { port } })

  for (let i = 0; i < replicaAmount; i++) {
    replicasList.push(await startReplica({ registry: { port }, default: true }))
  }

  for (let i = 0; i < subManagerAmount; i++) {
    subsManagersList.push(
      await startSubscriptionManager({ registry: { port } })
    )
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
    worker.on('message', message => {
      try {
        const obj = JSON.parse(message)
        if (obj.event === 'complete') {
          r(obj)
          worker.terminate()
        }
      } catch (_err) {}
    })
    const payload = JSON.stringify({
      event: 'start',
      payload: {
        fn: fn.toString(),
        time: opts.time
      }
    })

    worker.postMessage(payload)
  })
}

export async function run(
  fn: (client: SelvaClient) => void,
  opts: { time: number; clients: number } = { time: 5e3, clients: 5 }
): Promise<number> {
  const q = []
  for (let i = 0; i < opts.clients; i++) {
    q.push(startClient(fn, opts))
  }
  const results = await Promise.all(q)
  results.forEach((v, i) => {
    console.log('client', i, v.payload.time.length, 'iterations')
  })
  return 10
}
