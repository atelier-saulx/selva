import { getDiff, applyDiff } from 'recursive-diff'

export default (prev: any, newval: any) => {
  var d = Date.now()
  const x = getDiff(prev, newval)

  console.log('diff diff', x, Date.now() - d, 'ms')
}
