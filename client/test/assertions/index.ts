import t from 'ava'
import { Assertions } from 'ava/lib/assert.js'
import { logDb, dumpDb, idExists, wait } from './util'
import hash from '@sindresorhus/fnv1a'
import { join } from 'path'
import fs from 'fs'
import { Worker } from 'worker_threads'

declare module 'ava' {
  export interface Assertions {
    deepEqualIgnoreOrder(a: any, b: any, message?: string): boolean
  }
}

const deepSort = (a: any, b: any): void => {
  if (Array.isArray(a)) {
    if (typeof a[0] === 'object') {
      const s = (a, b) => {
        if (typeof a === 'object' && typeof b === 'object') {
          for (let k in a) {
            if (b[k] < a[k]) {
              return -1
            } else if (b[k] > a[k]) {
              return 1
            } else {
              return 0
            }
          }
        } else {
          return 0
        }
      }
      a.sort(s)
      b.sort(s)
    } else {
      a.sort()
      b.sort()
    }
    for (let i = 0; i < a.length; i++) {
      deepSort(a[i], b[i])
    }
  } else if (typeof a === 'object') {
    for (let k in a) {
      deepSort(a[k], b[k])
    }
  }
}

Object.assign(Assertions.prototype, {
  deepEqualIgnoreOrder(actual, expected, message = '') {
    deepSort(actual, expected)
    this.deepEqual(actual, expected, message)
  }
})

const tmp = join(__dirname, '../../tmp')

const worker = (fn: Function, context?: any): Promise<[any, Worker]> =>
  new Promise((resolve, reject) => {
    if (!fs.existsSync(tmp)) {
      fs.mkdirSync(tmp)
    }

    // fn has to be a async function
    const body = fn.toString()

    const script = context
      ? `
    const fn = ${body};
    const selvaServer = require('@saulx/selva-server')
    const selva = require('@saulx/selva')

    const p = {}

    for (let key in selva) {
      p[key] = selva[key]
    }

    for (let key in selvaServer) {
      p[key] = selvaServer[key]
    }

    const workers = require('worker_threads')
    workers.parentPort.on('message', (context) => {
      fn(p, context).then((v) => {
        workers.parentPort.postMessage(v);
      }).catch(err => {
        throw err
      })
    })
   
  `
      : `
      const fn = ${body};
      const selvaServer = require('@saulx/selva-server')
      const selva = require('@saulx/selva')
      const p = {}

      for (let key in selva) {
        p[key] = selva[key]
      }
  
      for (let key in selvaServer) {
        p[key] = selvaServer[key]
      }

      const workers = require('worker_threads')
      fn(p).then((v) => {
        workers.parentPort.postMessage(v);
      }).catch(err => {
        throw err
      })
    `

    const id = 'worker-' + hash(script) + '.js'

    const file = join(tmp, id)

    if (!fs.existsSync(join(tmp, id))) {
      fs.writeFileSync(join(tmp, id), script)
    }

    const worker = new Worker(file)

    if (context) {
      worker.postMessage(context)
    }

    worker.on('message', msg => {
      resolve([msg, worker])
    })

    worker.on('error', err => {
      reject(err)
    })
  })

export { logDb, dumpDb, idExists, wait, worker }
