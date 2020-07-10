const testAddon = require('./build/Release/testaddon.node')
const perfhooks = require('perf_hooks')
// const fastjson = require('fast-json-patch')
// const diffmpatch = require('diff-match-patch')
// const diff = require('./diff/meyers')

// const dmp = new diffmpatch()
// const diff =

const mybigobect = { flurp: [] }

for (let i = 0; i < 500; i++) {
  mybigobect.flurp.push({ i, flurp: 'HELLO BLAW' })
}

const things = []

for (let i = 0; i < 100000; i++) {
  const x = []
  x[0] = JSON.stringify(mybigobect)
  mybigobect.flurp[~~(Math.random() * 500)] = (~~(
    Math.random() * 100000
  )).toString(16)
  x[1] = JSON.stringify(mybigobect)
  things.push(x)
}

const applyPatch = (prevValue, patch, shouldbe) => {
  // inlcuding the parse its still a 100 times faster
  const arr = JSON.parse(patch)
  const len = arr.length
  let newStr = ''
  let cursor = 0
  for (let i = 0; i < len; i++) {
    const p = arr[i]
    const o = p[1]
    if (!o) {
      newStr += prevValue.slice(cursor, p[0] + cursor)
      cursor += p[0]
    } else if (o === 1) {
      newStr += p[0]
    } else if (o === 2) {
      cursor += p[0]
    }
  }
  if (newStr !== shouldbe) {
    console.error('NOOOO')
  }
  return newStr
}

const d3 = perfhooks.performance.now()
let x
for (let i = 0; i < things.length; i++) {
  const p = testAddon.hello(things[i][0], things[i][1])
  applyPatch(things[i][0], p, things[i][1])
  x = p
}
console.log(perfhooks.performance.now() - d3)

// console.log(x)
// const a = 'abcde {10} fg'
// const b = 'a FLAPdefg'

// applyPatch(a, testAddon.hello(a, b), b)

// first js then the rest

// const d2 = perfhooks.performance.now()
// for (let i = 0; i < things.length; i++) {
//   fastjson.compare(JSON.parse(things[i][0]), JSON.parse(things[i][1]))
// }
// console.log(perfhooks.performance.now() - d2)

module.exports = testAddon
