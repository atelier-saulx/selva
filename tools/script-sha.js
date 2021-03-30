const fs = require('fs')
const path = require('path')
const crypto = require('crypto')

const script = fs.readFileSync(
  path.join(__dirname, '..', 'client', 'dist', 'lua', 'update-schema.lua')
)

const shasum = crypto.createHash('sha1')
shasum.update(script)
console.log(shasum.digest().toString('hex'))

// CAN USE THIS TO CHECK SHA
// const redis = require('redis')
// const client = redis.createClient(6379)
// client.script('load', script, (err, val) => {
//   console.log('hmm', err, val)
//   process.exit(0)
// })
