function deepEqual(a, b) {
  if (a && b && typeof a == 'object' && typeof b == 'object') {
    if (Object.keys(a).length != Object.keys(b).length) return false
    for (var key in a) if (!deepEqual(a[key], b[key])) return false
    return true
  } else return a === b
}

const fs = require('fs')

const fst = process.argv[2]
const snd = process.argv[2]

if (!fst || !snd) {
  console.error('Must provide 2 file paths')
  process.exit(1)
}

const f1 = fs.readFileSync(fst, 'utf8')
const f2 = fs.readFileSync(snd, 'utf8')

const o1 = JSON.parse(f1)
const o2 = JSON.parse(f2)

console.log(deepEqual(o1, o2))
