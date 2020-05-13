import { GetOptions } from '@saulx/selva'

import { Worker } from 'worker_threads'

import path from 'path'

import { SelvaServer } from '..'
import { ServerOptions } from '../../types'

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

export default class SubWorker {
  public worker: Worker
  public server: SelvaServer

  destroy() {
    return new Promise(resolve => {
      console.log('Destroy subs worker')
      this.worker.once('destroyComplete', async () => {
        console.log('Destroy complete!')
        this.worker.removeAllListeners()
        this.worker = null
        resolve()
      })
      this.worker.postMessage(JSON.stringify({ event: 'destroy' }))
    })
  }
  async connect(opts: ServerOptions): Promise<void> {
    // also pass port, and id
    return new Promise(resolve => {
      const worker = (this.worker = new Worker(
        path.join(__dirname, '/worker.js')
      ))
      worker.once('connect', () => {
        resolve()
      })
      worker.on('message', message => {
        try {
          const obj = JSON.parse(message)
          if (obj.event) {
            worker.emit(obj.event, obj.payload)
          }
        } catch (_err) {}
      })
      this.worker.postMessage(
        JSON.stringify({ event: 'connect', payload: opts })
      )
    })
  }
}
