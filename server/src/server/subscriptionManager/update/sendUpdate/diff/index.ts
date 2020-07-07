import * as jsonpatch from 'fast-json-patch'
import { performance } from 'perf_hooks'

export default (prev: any, newval: any) => {
  var d = performance.now()

  if (prev !== null && newval !== null) {
    const y = jsonpatch.compare(JSON.parse(prev), JSON.parse(newval))
    const x = performance.now() - d
    console.log('diff diff!xxx!!', y, x)
  }
}
