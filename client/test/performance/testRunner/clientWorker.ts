import { connect, SelvaClient } from '../../../src/index'
import { parentPort } from 'worker_threads'
let client: SelvaClient
parentPort.on('message', (message: string) => {
  try {
    const { event, payload } = JSON.parse(message)
    if (event === 'start') {
      client = connect({ port: payload.port })
      setTimeout(() => {
        const result: { time: number[]; startTime: number } = {
          time: [],
          startTime: Date.now()
        }
        const fn = new Function(payload.fn)
        console.log(fn)
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
