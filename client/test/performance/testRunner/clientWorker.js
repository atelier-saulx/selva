const { connect } = require('../../../dist/src/index')
const { parentPort } = require('worker_threads')

const { promisify } = require('util')
const { exec } = require('child_process')
const execPromise = promisify(exec)

let client
parentPort.on('message', message => {
  try {
    const { event, payload } = JSON.parse(message)

    if (event === 'start') {
      client = connect({ port: payload.port })

      const result = {
        time: [],
        startTime: Date.now()
      }

      client.on('connect', () => {
        console.info('connect!')
        // eslint-disable-next-line
        const fn = new Function('client', 'execPromise', payload.fn)
        const setLoop = async () => {
          const s = Date.now()
          await fn(client, execPromise)
          result.time.push(Date.now() - s)
          if (Date.now() - result.startTime > payload.time) {
            parentPort.postMessage(
              JSON.stringify({
                event: 'complete',
                payload: result
              })
            )
            client.destroy()
          } else {
            setLoop()
          }
        }

        setLoop()
      })
    }
  } catch (_err) {}
})
