
      const fn = async ({ connect }) => {
        const client = connect({ port: 9999 });
        const r = await client.redis.hgetall('flappie');
        console.log(r);
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
      fn(p).then((v) => {
        workers.parentPort.postMessage(v);
      }).catch(err => {
        throw err
      })
    