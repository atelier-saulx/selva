
      const fn = async () => {
        console.log('ok exec on worker!');
        return { x: true };
        throw new Error('snurf');
    };
      const selvaServer = require('@saulx/selva-server')
      const selva = require('@saulx/selva')
      const workers = require('worker_threads')
      fn().then((v) => {
        workers.parentPort.postMessage(v);
      }).catch(err => {
        throw err
      })
    