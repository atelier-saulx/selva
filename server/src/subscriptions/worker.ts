import SubsManager from './subsManager'
import { ConnectOptions } from '@saulx/selva'

const { parentPort } = require('worker_threads')

console.log('start subs worker')
const subsManager = new SubsManager()

parentPort.on('message', message => {
  try {
    const { event, payload } = JSON.parse(message)
    if (event === 'connect') {
      console.log('START')
      subsManager.connect(<ConnectOptions>payload).then(() => {
        parentPort.postMessage(
          JSON.stringify({
            event: 'connect'
          })
        )
      })
    } else if (event === 'destroy') {
      console.log('destroy subs client')
      subsManager.destroy()
      parentPort.postMessage(
        JSON.stringify({
          event: 'destroyComplete'
        })
      )
    }
  } catch (_err) {}
})
