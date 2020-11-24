const { performance } = require('perf_hooks')
const { connect } = require('@saulx/selva')
const { start } = require('@saulx/selva-server')
const getPort = require('get-port')
const { RateLimit } = require('async-sema')

const { Worker, isMainThread, workerData } = require('worker_threads')

let srv
let port

async function before() {
  port = await getPort()
  srv = await start({
    port
  })

  const client = connect({ port })

  // await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      show: {
        prefix: 'sh',
        fields: {
          title: { type: 'text' },
          votes: { type: 'number' }
        }
      },
      vote: {
        prefix: 'vo',
        fields: {
          uid: { type: 'string' }
        }
      }
    }
  })

  await client.set({
    $id: 'root',
    children: [
      {
        $id: 'sh1',
        $language: 'en',
        type: 'show',
        title: 'LOL',
        votes: 0
      },
      {
        $id: 'sh2',
        $language: 'en',
        type: 'show',
        title: 'ROFL',
        votes: 0
      }
    ]
  })

  await client.redis.selva_subscriptions_add('___selva_hierarchy', 'aa35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b93', 1, 'children', 'fields', 'votes')
}

async function after() {
  const client = connect({ port })
  const d = Date.now()
  //await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
}

const nrVotes = 500000
const votesPerSecond = 5000
const nrWorkers = 3

let sh = ['sh1', 'sh2']

async function runWorker() {
  const client = connect({ port: workerData.port }, { loglevel: 'info' })

  const votes = Array.from(Array(nrVotes).keys()).map(v => ({
    $id: sh[v & 1],
    votes: { $increment: 1 },
    children: {
      $add: [{ type: 'vote', uid: `user${v}` }]
    }
  }))

  const lim = RateLimit(votesPerSecond, { timeUnit: 1000 })
  await Promise.all(
    votes.map(async vote => {
      await lim()
      return client.set(vote)
    })
  )
}

async function run() {
  if (isMainThread) {
    await before()
    // TODO: await after() in the right place

    const start = performance.now()
    const workers = []
    for (let i = 0; i < nrWorkers; i++) {
      workers.push(
        new Promise((resolve, reject) => {
          const worker = new Worker(__filename, {
            workerData: { port }
          })
          worker.on('message', resolve)
          worker.on('error', reject)
          worker.on('exit', code => {
            if (code !== 0) {
              return reject(new Error(`Worker exited with code ${code}`))
            }

            resolve()
          })
        })
      )
    }

    await Promise.all(workers)

    const end = performance.now()
    const tTotal = end - start
    console.log('tTotal:', tTotal / 1000)
  } else {
    await runWorker()
  }
}

async function getStats() {
  const client = connect({ port }, { loglevel: 'info' })
  console.log(
    await client.get({
      shows: {
        id: true,
        title: true,
        votes: true,
        $list: {
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'show'
              }
            ]
          }
        }
      }
    })
  )
}

run()
  .then(() => {
    if (!isMainThread) {
      process.exit(0)
    }
  })
  .then(() => new Promise(r => setTimeout(r, 1e3)))
  .then(() => {
    return getStats()
  })
  .then(() => {
    console.log('Success!')
    process.exit(0)
  })
  .catch(e => {
    console.error(e)
    process.exit(1)
  })
