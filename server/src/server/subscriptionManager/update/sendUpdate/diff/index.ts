import * as jsonpatch from 'fast-json-patch'
import { performance } from 'perf_hooks'
import { Worker } from 'worker_threads'
import { join } from 'path'

const workerPath = join(__dirname, 'worker.js')

// start with a worker

// async is not the best here either...

console.log('SNURFELSTEIN')
var workerAmount = 2
const workers = []
let workerIndex = 0

for (let i = 0; i < workerAmount; i++) {
  workers.push(new Worker(workerPath))
}

// have to make a c++ plugin for this
export default (prev: any, newval: any) =>
  new Promise(resolve => {
    var d = performance.now()

    if (prev !== null && newval !== null) {
      workerIndex++
      if (workerIndex > workers.length - 1) {
        workerIndex = 0
      }
      const worker = workers[workerIndex]
      // nasty stringify
      worker.postMessage({ prev, newval })
      worker.once('message', message => {
        const x = performance.now() - d
        console.log('dif speed', x)
        resolve(message)
      })
    } else {
      resolve()
    }
  })
