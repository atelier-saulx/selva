
    const fn = async (selva, context) => {
        console.log('ok exec on worker!', selva, context);
        return { x: true };
    };
    const selvaServer = require('@saulx/selva-server')
    const selva = require('@saulx/selva')

    const p = {}

    for (let key in selva) {
      p[key] = selva[key]
    }

    for (let key in selvaServer) {
      p[key] = selvaServer[key]
    }

    const workers = require('worker_threads')
    workers.parentPort.on('message', (context) => {
      fn(p, context).then((v) => {
        workers.parentPort.postMessage(v);
      }).catch(err => {
        throw err
      })
    })
   
  