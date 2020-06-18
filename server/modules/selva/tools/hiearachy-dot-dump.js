const redis = require('redis')
const {promisify} = require('util')

redis.add_command('SELVA.HIERARCHY.dump')
const r = redis.createClient(6379, '127.0.0.1')
const dump = promisify(r['SELVA.HIERARCHY.dump']).bind(r)

async function run() {
  const [_, __, ...args] = process.argv;
  const arr = await promisify(r['SELVA.HIERARCHY.dump']).bind(r)(...args)

  console.log('digraph {')
  for (const sub of arr) {
    const [node, ...children] = sub
    for (const child of children) {
      console.log(`  "n${node.replace('\0', ' ')}" -> "n${child.replace('\0', ' ')}"`)
    }
  }
  console.log('}')

}

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

run()
  .catch(e => {
    console.error(e)
  })
  .finally(() => {
    sleep(5000).then(() => process.exit(0))
  })
