const { connect } = require('../../../dist/src/index')
const { parentPort } = require('worker_threads')

const { promisify } = require('util')
const { exec } = require('child_process')
const execPromise = promisify(exec)

const wait = (time = 100) => new Promise(resolve => setTimeout(resolve, time))

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
        const index = payload.index
        // eslint-disable-next-line
        const fn = new Function(
          'client',
          'index',
          'execPromise',
          'wait',
          payload.fn
        )
        const setLoop = async () => {
          const s = Date.now()
          await fn(client, index, execPromise, wait)
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
