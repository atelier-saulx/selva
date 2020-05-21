const { connect } = require('../../../dist/src/index')
const { parentPort } = require('worker_threads')

let client
parentPort.on('message', message => {
  try {
    const { event, payload } = JSON.parse(message)

    if (event === 'start') {
      client = connect({ port: payload.port })

      setTimeout(() => {
        const result = {
          time: [],
          startTime: Date.now()
        }
        // eslint-disable-next-line
        const fn = new Function(payload.fn)
        const setLoop = async () => {
          const s = Date.now()
          await fn(client)
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
      }, 0)
    }
  } catch (_err) {}
})
