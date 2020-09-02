
  const fn = async () => {
        console.log('ok exec on worker!');
    };
  const selvaServer = require('@saulx/selva-server')
  const selva = require('@saulx/selva')
  const workers = require('worker_threads')

  console.log('yesh in the worker')
  fn().then(() => {
    console.log('did it!')
    workers.parentPort.postMessage(true);
  }).catch(err => {
    workers.parentPort.postMessage(err);
  })
