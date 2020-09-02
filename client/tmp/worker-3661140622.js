
    const fn = async ({ selva, selvaServer }, context) => {
        console.log('ok exec on worker!', selva, selvaServer, context);
        return { x: true };
    };
    const selvaServer = require('@saulx/selva-server')
    const selva = require('@saulx/selva')
    const workers = require('worker_threads')
    workers.parentPort.on('message', (context) => {
      fn({ selva, selvaServer }, context).then((v) => {
        workers.parentPort.postMessage(v);
      }).catch(err => {
        throw err
      })
    })
   
  