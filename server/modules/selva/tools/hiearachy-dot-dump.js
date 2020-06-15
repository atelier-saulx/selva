const redis = require('redis')
const {promisify} = require('util')

redis.add_command('SELVA.HIERARCHY.dump')
const r = redis.createClient(6379, '127.0.0.1')
const dump = promisify(r['SELVA.HIERARCHY.dump']).bind(r)

async function run() {
  const arr = await promisify(r['SELVA.HIERARCHY.dump']).bind(r)(process.argv[2])

  console.log('digraph {')
  for (const sub of arr) {
    const [node, ...children] = sub
    for (const child of children) {
      console.log(`  ${node} -> ${child}`)
    }
  }
  console.log('}')

}

run()
  .catch(e => {
    console.error(e)
  })
  .finally(() => {
    process.exit(0)
  })
