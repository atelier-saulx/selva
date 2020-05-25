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
let subManagerAmount = 10

export async function start({ replicas = 0, subsManagers = 0 } = {}): Promise<{
  registry: SelvaServer
}> {
  replicaAmount = replicas
  subsManagers = subsManagers
  const port = await getPort()
  // small test
  registry = await startRegistry({
    port
  })

  // attach yourself
  origin = await startOrigin({
    registry: { port },
    default: true,
    port: 6379
  })

  origin.pm.on('stdout', log => {
    console.log(log)
  })

  const client = connect({ port })

  await client.updateSchema({
    rootType: {
      fields: { value: { type: 'number' } }
    },
    types: {
      thing: {
        prefix: 'th',
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
  fn: string,
  opts: { label?: string; time: number; clients: number }
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
        fn,
        time: opts.time,
        port: registry.port
      }
    })

    worker.postMessage(payload)
  })
}

export async function run(
  fn: (client: SelvaClient) => void,
  opts: { label?: string; time: number; clients: number } = {
    time: 5e3,
    clients: 5
  }
): Promise<{ total: number; mean: number; iterations: number }> {
  const q = []
  const fnString = `return new Promise(r => {  
    const x = ${fn.toString()}
    x(client).then(r)  
  })`

  console.log(
    chalk.blue(
      `Start performance test with ${chalk.white(
        opts.clients
      )} clients for ${chalk.white(opts.time / 1000)} seconds`
    )
  )

  for (let i = 0; i < opts.clients; i++) {
    q.push(startClient(fnString, opts))
  }
  const results = await Promise.all(q)

  let mean: number
  let total = 0
  let label = opts.label || ''
  let totalIterations = 0

  results.forEach((v, i) => {
    totalIterations += v.payload.time.length
    v.payload.time.forEach(t => {
      total += t
    })
  })

  mean = total / totalIterations

  let totalPerClient = total / opts.clients

  console.log(
    chalk.blue(
      `${label || 'Test'} ops ${chalk.white(
        totalIterations / (totalPerClient / 1e3)
      )} / second`
    )
  )

  return { total: totalPerClient, mean, iterations: totalIterations }
}
