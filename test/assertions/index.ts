import t from 'ava'
import { Assertions } from 'ava/lib/assert.js'
import { logDb, dumpDb, idExists } from './util'

declare module 'ava' {
  export interface Assertions {
    deepEqualIgnoreOrder(a: any, b: any): boolean
  }
}

const deepSort = (a: any, b: any): boolean => {
  if (typeof a !== typeof b) {
    return false
  }
  if (Array.isArray(a)) {
    if (!Array.isArray(b)) {
      // return false
    }
    if (a.length !== b.length) {
      // return false
    }
    a.sort()
    b.sort()
    for (let i = 0; i < a.length; i++) {
      if (!deepSort(a[i], b[i])) {
        // return false
      }
    }
  } else if (typeof a === 'object') {
    if (Object.keys(a).length !== Object.keys(b).length) {
      // return false
    }
    for (let k in a) {
      if (!deepSort(a[k], b[k])) {
        // return false
      }
    }
  } else {
    if (a !== b) {
      // return false
    }
  }
  // return true
}

Object.assign(Assertions.prototype, {
  deepEqualIgnoreOrder(actual, expected, message = '') {
    deepSort(actual, expected)
    this.deepEqual(actual, expected, message)
  }
})

export { logDb, dumpDb, idExists }
