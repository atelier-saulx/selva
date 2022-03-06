import t from 'ava'
import { Assertions } from 'ava/lib/assert.js'
import { idExists, wait } from './util'
import hash from '@sindresorhus/fnv1a'
import { join } from 'path'
import fs from 'fs'
import { Worker } from 'worker_threads'
import rimraf from 'rimraf'
import beforeExit from 'before-exit'
import { connections } from '@saulx/selva'
import chalk from 'chalk'
import { deepCopy } from '@saulx/utils'

declare module 'ava' {
  export interface Assertions {
    deepEqualIgnoreOrder(a: any, b: any, message?: string): boolean
    connectionsAreEmpty(): Promise<void>
  }
}

const deepSort = (a: any, b: any): void => {
  if (Array.isArray(a)) {
    if (typeof a[0] === 'object') {
      const s = (a, b) => {
        if (typeof a === 'object' && typeof b === 'object') {
          if (a.id && b.id) {
            if (b.id < a.id) {
              return -1
            } else if (b.id > a.id) {
              return 1
            } else {
              return 0
            }
          }
          // eslint-disable-next-line
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
  async connectionsAreEmpty() {
    if (connections.size === 0) {
      console.log(chalk.grey('    Connections are empty'))
      this.pass('Connections are empty')
      return
    } else {
      for (let i = 0; i < 60; i++) {
        await wait(1e3)
        if (connections.size === 0) {
          console.log(
            chalk.grey('    Connections are empty after ' + (i + 1) + 's')
          )
          this.pass('Connection are empty after ' + (i + 1) + 's')
          return
        }
      }
    }
    this.fail(
      'Connection are not empty after 1 min, remaining: ' + connections.size
    )
  },
})

Object.assign(Assertions.prototype, {
  deepEqualIgnoreOrder(actual, expected, message = '') {
    const actCopy = deepCopy(actual)
    const expCopy = deepCopy(expected)
    deepSort(actCopy, expCopy)
    this.deepEqual(actCopy, expCopy, message)
  },
})

const tmp = join(__dirname, '../../tmp')

const worker = (
  fn: Function,
  context?: any
): Promise<[any, Worker, () => void]> =>
  new Promise((resolve, reject) => {
    if (!fs.existsSync(tmp)) {
      fs.mkdirSync(tmp)
    }

    // fn has to be a async function
    const body = fn.toString()

    const script = `
      const fn = ${body};
      global.isWorker = true

      const selvaServer = require('@saulx/selva-server')
      const selva = require('@saulx/selva')
      const wait = (t = 100) => (new Promise(r => setTimeout(r, t)))

      const p = { wait }


      for (let key in selva) {
        p[key] = selva[key]
      }

      for (let key in selvaServer) {
        p[key] = selvaServer[key]
      }

      const { workerData, parentPort } = require('worker_threads')
      let cleanup
      fn(p, workerData).then((v) => {
        if (typeof v === 'function') {
          cleanup = v
          v = null
        }
        parentPort.postMessage(v);
      }).catch(err => {
        throw err
      })

      parentPort.on('message', async (msg) => {
        if (msg === '___KILL___') {
          if (cleanup) {
            await cleanup()
            await wait(500)
          }
          process.exit()
        }
      })
    `

    const id = 'worker-' + hash(script) + '.js'

    const file = join(tmp, id)

    if (!fs.existsSync(join(tmp, id))) {
      fs.writeFileSync(join(tmp, id), script)
    }

    const worker = new Worker(file, { workerData: context || {} })
    beforeExit.do(() => {
      try {
        console.log('Before exit hook close worker')
        worker.terminate()
      } catch (_err) {}
    })

    const kill = () => {
      worker.postMessage('___KILL___')
    }

    worker.on('message', (msg) => {
      resolve([msg, worker, kill])
    })

    worker.on('error', (err) => {
      reject(err)
    })
  })

const removeDump = (dir: string) => {
  return async () => {
    if (fs.existsSync(dir)) {
      rimraf(dir, (err) => {
        if (err) {
          console.log('cannot remove dump')
        }
      })
    }
    await wait(1e3)
  }
}

export { idExists, wait, worker, removeDump }
