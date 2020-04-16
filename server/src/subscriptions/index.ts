import { GetOptions, ConnectOptions } from '@saulx/selva'
import { Subscriptions } from '../'

import { Worker } from 'worker_threads'

import path from 'path'

import { startInternal, SelvaServer } from '..'

export type Tree = Record<string, any>

export type SubTree = Record<string, any>

export type Subscription = {
  clients: Set<string>
  get: GetOptions
  version?: string
  tree?: SubTree
  treeVersion?: string
  inProgress?: boolean
}

export default class SubWorker {
  public worker: Worker
  public server: SelvaServer

  async createServer(subscriptions: Subscriptions) {
    this.server = await startInternal({
      port: subscriptions.port,
      service: subscriptions.service,
      subscriptions: false,
      verbose: true
    })
  }

  destroy() {
    return new Promise(resolve => {
      console.log('Destroy subs worker')
      this.worker.once('destroyComplete', async () => {
        console.log('Destroy complete!')
        this.worker.removeAllListeners()
        this.worker = null
        if (this.server) {
          await this.server.destroy()
        }
        resolve()
      })
      this.worker.postMessage(JSON.stringify({ event: 'destroy' }))
    })
  }
  async connect(opts: Subscriptions): Promise<void> {
    const subscriptions = opts.selvaServer.service
      ? opts.selvaServer.service instanceof Promise
        ? await opts.selvaServer.service
        : opts.selvaServer.service
      : {
          port:
            opts.selvaServer.port instanceof Promise
              ? await opts.selvaServer.port
              : opts.selvaServer.port,

          host:
            opts.selvaServer.host instanceof Promise
              ? await opts.selvaServer.host
              : opts.selvaServer.host
        }

    const connectOptions = {
      port: subscriptions.port,
      host: subscriptions.host,
      subscriptions: {
        port: opts.port instanceof Promise ? await opts.port : opts.port,
        host: opts.host instanceof Promise ? await opts.host : opts.host
      }
    }

    console.log('connect opts', connectOptions)

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
        JSON.stringify({ event: 'connect', payload: connectOptions })
      )
    })
  }
}
