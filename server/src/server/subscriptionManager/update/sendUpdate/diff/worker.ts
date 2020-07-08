import * as jsonpatch from 'fast-json-patch'
import { parentPort } from 'worker_threads'
// import gzip

console.info('Start diff worker')

parentPort.once('message', message => {
  const { prev, newval } = message
  // so slow... sad
  const y = jsonpatch.compare(JSON.parse(prev), JSON.parse(newval))
  // zlib.gzip(buffer[, options], callback)
  // can also use this
  parentPort.postMessage(JSON.stringify(y))
})
