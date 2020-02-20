import t from 'ava'
import { Assertions } from 'ava/lib/assert.js'
import { logDb, dumpDb, idExists, wait } from './util'

declare module 'ava' {
  export interface Assertions {
    deepEqualIgnoreOrder(a: any, b: any, message?: string): boolean
  }
}

const deepSort = (a: any, b: any): void => {
  if (Array.isArray(a)) {
    a.sort()
    b.sort()
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

export { logDb, dumpDb, idExists, wait }
