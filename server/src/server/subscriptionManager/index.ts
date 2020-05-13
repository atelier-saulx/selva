import { GetOptions } from '@saulx/selva'
import { ServerOptions } from '../../types'

import { Worker } from 'worker_threads'
import path from 'path'

export type Tree = Record<string, any>

export type SubTree = Record<string, any>

export type RefreshSubscriptions = {
  nextRefresh: number
  subscriptions: Subscription[]
}

export type Subscription = {
  clients: Set<string>
  get: GetOptions
  version?: string
  tree?: SubTree
  treeVersion?: string
  inProgress?: boolean
  channel: string
  refreshAt?: number
}

export const startSubscriptionManager = (
  opts: ServerOptions
): Promise<Worker> => {
  return new Promise(resolve => {
    const worker = (this.worker = new Worker(
      path.join(__dirname, '/worker.js')
    ))
    worker.once('connect', () => {
      resolve(worker)
    })
    worker.on('message', message => {
      try {
        const obj = JSON.parse(message)
        if (obj.event) {
          worker.emit(obj.event, obj.payload)
        }
      } catch (_err) {}
    })
    worker.postMessage(JSON.stringify({ event: 'connect', payload: opts }))
  })
}

export const stopSubscriptionManager = (worker: Worker): Promise<void> => {
  return new Promise(resolve => {
    console.log('Destroy subs worker')
    worker.once('destroyComplete', async () => {
      console.log('Destroy complete!')
      worker.removeAllListeners()
      resolve()
    })
    worker.postMessage(JSON.stringify({ event: 'destroy' }))
  })
}
