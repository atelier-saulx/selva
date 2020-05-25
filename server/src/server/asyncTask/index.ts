import { Worker } from 'worker_threads'
import * as path from 'path'

let worker: Worker

export function startAsyncTaskWorker(): void {
  worker = new Worker(path.join(__dirname, '/worker.js'))
}

export function stopAsyncTaskWorker(): Promise<number> {
  if (worker) {
    return worker.terminate()
  }

  return Promise.resolve(0)
}
