const testAddon = require('./build/Release/testaddon.node')
const perfhooks = require('perf_hooks')
const fastjson = require('fast-json-patch')
const { compare } = require('./diff/objectDiff')
const diffmpatch = require('diff-match-patch')
// const diff = require('./diff/meyers')

const dmp = new diffmpatch()

// console.log(dmp)
// const diff =

const mybigobect = { flurp: [] }

for (let i = 0; i < 500; i++) {
  mybigobect.flurp.push({ i, flurp: 'HELLO BLAW' })
}

const things = []

for (let i = 0; i < 10000; i++) {
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
  // const p = testAddon.hello(things[i][0], things[i][1])
  // applyPatch(things[i][0], p, things[i][1])
  const p = dmp.diff_main(things[i][0], things[i][1])
  x = p
}

console.log(perfhooks.performance.now() - d3)

console.log(x)

let docs = []
for (let i = 0; i < 10000; i++) {
  const doc1 = {
    arr: [],
    title: ~~(Math.random() * 100) + 'aaaa'
  }
  const doc2 = {
    arr: [],
    title: ~~(Math.random() * 100) + 'aaaa'
  }

  for (let i = 0; i < ~~(Math.random() * 10000); i++) {
    doc1.arr.push({ flap: ~~(Math.random() * 10), x: true })
  }

  for (let i = 0; i < ~~(Math.random() * 10000); i++) {
    doc2.arr.push({ flap: ~~(Math.random() * 10), x: true })
  }

  docs.push([doc1, doc2])
}

let dx = perfhooks.performance.now()

let y
for (let i = 0; i < docs.length; i++) {
  const [a, b] = docs[i]
  // y = fastjson.compare(a, b, true)
  y = compare(a, b)
}

x = JSON.stringify(y)

console.log(perfhooks.performance.now() - dx, x.length)

// const a = compare(docs[0][0], docs[0][1])
// console.log('---->', y)

module.exports = testAddon
