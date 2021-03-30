const fs = require('fs')
const path = require('path')
const crypto = require('crypto')

const script = fs.readFileSync(
  path.join(__dirname, '..', 'client', 'dist', 'lua', 'update-schema.lua')
)

const shasum = crypto.createHash('sha1')
shasum.update(script)
const sha = shasum.digest().toString('hex')
console.log(sha)

const content = fs.readFileSync(
  path.join(__dirname, '..', 'client', 'src', 'connection', 'scripts.ts'),
  'utf8'
)

const START_STR = '/* <BEGIN_INSERT_SCRIPTS */\n'
const END_STR = '  /* <END_INSERT_SCRIPTS */\n'
const startIdx = content.indexOf(START_STR) + START_STR.length
const endIdx = content.indexOf(END_STR)

const keys = `  'update-schema': '${sha}',\n`
const transformed =
  content.slice(0, startIdx) + keys + content.slice(endIdx, content.length)

console.log(transformed)
fs.writeFileSync(
  path.join(__dirname, '..', 'client', 'src', 'connection', 'scripts.ts'),
  transformed
)

// CAN USE THIS TO CHECK SHA
// const redis = require('redis')
// const client = redis.createClient(6379)
// client.script('load', script, (err, val) => {
//   console.log('hmm', err, val)
//   process.exit(0)
// })
