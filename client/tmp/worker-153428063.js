
      const fn = async (selva, server) => {
        console.log('ok exec on worker!', selva, server);
        return { x: true };
    };
      const selvaServer = require('@saulx/selva-server')
      const selva = require('@saulx/selva')
      const workers = require('worker_threads')
      fn(selva, selvaServer).then((v) => {
        workers.parentPort.postMessage(v);
      }).catch(err => {
        throw err
      })
    