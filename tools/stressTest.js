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
    port,
  })

  const client = connect({ port })

  await client.updateSchema({
    languages: ['en'],
    types: {
      show: {
        prefix: 'sh',
        fields: {
          title: { type: 'text' },
          votes: { type: 'number' },
        },
      },
      vote: {
        prefix: 'vo',
        fields: {
          uid: { type: 'string' },
        },
      },
    },
  })

  await Promise.all([
    client.set({
      $id: 'sh1',
      $language: 'en',
      type: 'show',
      title: 'LOL',
      votes: 0,
    }),
    client.set({
      $id: 'sh2',
      $language: 'en',
      type: 'show',
      title: 'ROFL',
      votes: 0,
    }),
  ])
}

const nrVotes = 600
const votesPerSecond = 10000
const nrWorkers = 1

let sh = ['sh1', 'sh2']

async function runWorker() {
  const client = connect({ port: workerData.port }, { loglevel: 'info' })

  let i = 0
  while (true) {
    const votes = Array.from(Array(nrVotes).keys()).map((v) => ({
      $id: sh[v & 1],
      votes: { $increment: 1 },
      children: {
        $add: [{ type: 'vote', uid: `user${v}` }],
      },
    }))

    const lim = RateLimit(votesPerSecond, { timeUnit: 1000 })
    await Promise.all(
      votes.map(async (vote) => {
        await lim()
        return client.set(vote)
      })
    )

    const c = Math.abs(i)

    if (c & 1) {
      const uid = `user${c % nrVotes}`
      console.log(`deleting votes of ${uid}`)
      const { votes: rmVotes } = await client.get({
        votes: {
          id: true,
          $list: {
            $find: {
              $traverse: 'descendants',
              $filter: {
                $operator: '=',
                $field: 'type',
                $value: 'vote',
                $and: {
                  $operator: '=',
                  $field: 'uid',
                  $value: uid,
                },
              },
            },
          },
        },
      })
      await Promise.all(rmVotes.map(({ id }) => client.delete(id)))
    }

    if (c % 100 === 0 || c % 201 === 0) {
      const $id = sh[c & 1]
      console.log(`deleting children of ${$id}`)
      const { children: ids } = await client.get({ $id, children: true })
      await Promise.all(ids.map((id) => client.delete(id)))
      await client.set({
        $id,
        votes: 0,
      })
    }
    i++
  }
}

async function run() {
  if (isMainThread) {
    await before()

    const start = performance.now()
    const workers = []
    for (let i = 0; i < nrWorkers; i++) {
      workers.push(
        new Promise((resolve, reject) => {
          const worker = new Worker(__filename, {
            workerData: { port },
          })
          worker.on('message', resolve)
          worker.on('error', reject)
          worker.on('exit', (code) => {
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

run()
  .then(() => {
    if (!isMainThread) {
      process.exit(0)
    }
  })
  .then(() => {
    console.log('End!')
    process.exit(0)
  })
  .catch((e) => {
    console.error(e)
    process.exit(1)
  })
