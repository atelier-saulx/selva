const testAddon = require('./build/Release/testaddon.node')
const perfhooks = require('perf_hooks')
const fastjson = require('fast-json-patch')
const diffmpatch = require('diff-match-patch')
// const diff = require('diff)

const dmp = new diffmpatch()
// const diff =

const mybigobect = { flurp: [] }

for (let i = 0; i < 100; i++) {
  mybigobect.flurp.push({ i, flurp: 'HELLO BLAW' })
}

const things = []

for (let i = 0; i < 1000; i++) {
  const x = []
  x[0] = JSON.stringify(mybigobect)
  mybigobect.flurp[~~(Math.random() * 99)] = (~~(
    Math.random() * 100000
  )).toString(16)
  x[1] = JSON.stringify(mybigobect)
  things.push(x)
}

const d = perfhooks.performance.now()
for (let i = 0; i < things.length; i++) {
  dmp.diff_main(things[i][0], things[i][1])
  //   testAddon.hello(things[i][0], things[i][1])
}
console.log(perfhooks.performance.now() - d)

let a
const d3 = perfhooks.performance.now()
for (let i = 0; i < things.length; i++) {
  //   dmp.diff_main(things[i][0], things[i][1])
  a = testAddon.hello(things[i][0], things[i][1])
}
console.log(perfhooks.performance.now() - d3)

console.log(a)

const d2 = perfhooks.performance.now()
for (let i = 0; i < things.length; i++) {
  fastjson.compare(JSON.parse(things[i][0]), JSON.parse(things[i][1]))
}
console.log(perfhooks.performance.now() - d2)

module.exports = testAddon
