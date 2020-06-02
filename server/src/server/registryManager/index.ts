import { ServerOptions } from '../../types'
import { SubscriptionManagerState } from './types'
import { Worker } from 'worker_threads'
import path from 'path'

const connect = async (
  state: SubscriptionManagerState,
  opts: ServerOptions
) => {
  // TODO: Fix changing url of registry
  if (typeof opts.registry === 'function') {
    opts.registry = await opts.registry()
  } else if (opts.registry instanceof Promise) {
    opts.registry = await opts.registry
  }
  state.worker.postMessage(JSON.stringify({ event: 'connect', payload: opts }))
}

export const registryManager = (
  opts: ServerOptions,
  state: SubscriptionManagerState = {}
): Promise<SubscriptionManagerState> => {
  return new Promise(resolve => {
    const worker = (this.worker = new Worker(
      path.join(__dirname, '/worker.js')
    ))
    state = { worker }
    worker.once('connect', () => resolve(state))
    worker.on('message', message => {
      try {
        const obj = JSON.parse(message)
        if (obj.event) {
          worker.emit(obj.event, obj.payload)
        }
      } catch (_err) {}
    })
    connect(state, opts)
  })
}

export const stopRegistryManager = (
  state: SubscriptionManagerState
): Promise<void> => {
  return new Promise(resolve => {
    state.worker.once('destroyComplete', async () => {
      state.worker.removeAllListeners()
      delete state.worker
      resolve()
    })
    state.worker.postMessage(JSON.stringify({ event: 'destroy' }))
  })
}

export { SubscriptionManagerState }
