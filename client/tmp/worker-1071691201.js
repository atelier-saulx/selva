
    const fn = async () => {
        console.log('ok exec on worker!');
    };
    const selvaServer = require('@saulx/selva-server')
    const selva = require('@saulx/selva')

    console.log('yesh in the worker')
    fn().then(() => {
      console.log('did it!')
    }).catch(err => {
      console.error('Error in worker', err)
    })
  