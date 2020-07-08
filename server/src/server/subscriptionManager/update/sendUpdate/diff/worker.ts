const jsonpatch = require('fast-json-patch')
const { parentPort } = require('worker_threads')
// import gzip

// const testAddon = require('./build/Release/testaddon.node')

const testAddon = require('../../../../../../../build/Release/testaddon.node')

console.info('Start diff worker!2')

parentPort.once('message', message => {
  const { prev, newval } = message
  const y = testAddon.hello(prev, newval)
  // const y = jsonpatch.compare(JSON.parse(prev), JSON.parse(newval))
  parentPort.postMessage(y)
})
