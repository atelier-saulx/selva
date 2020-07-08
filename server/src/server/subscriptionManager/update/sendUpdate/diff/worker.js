// import * as jsonpatch from 'fast-json-patch'
const { parentPort } = require('worker_threads')
// import gzip
const testAddon = require('../../../../../../../build/Release/testaddon.node')

console.info('Start diff worker')

parentPort.once('message', message => {
  const { prev, newval } = message
  // so slow... sad

  console.log('go')
  const y = testAddon.hello(prev, newval)
  console.log(y)
  //   const y = jsonpatch.compare(JSON.parse(prev), JSON.parse(newval))
  // zlib.gzip(buffer[, options], callback)
  // can also use this
  parentPort.postMessage(y)
})
