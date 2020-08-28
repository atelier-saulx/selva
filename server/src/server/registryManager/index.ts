import { SelvaServer } from '../'
import { constants } from '@saulx/selva'

export const registryManager = (server: SelvaServer): Promise<void> => {
  // also subscribe on some stuff

  return new Promise(() => {
    // listener on registry update info
    // if info < 30 seconds unregister server
    // this will becomes very different

    console.log('its the registry manager!')

    // add server

    server.selvaClient.on('added-servers', ({ event, server }) => {
      console.log('got new server')
      // this means we are going to re-index

      if (event === '*') {
        // got all of them
        console.log('initial servers')
      } else {
        console.log('individual is added!', server)
      }
    })

    // less important only relevant potentialy for an emit before shutdown...
    // server.selvaClient.on('remove-servers', ({ event }) => {
    //   console.log('got remove server')
    // })

    const cleanIdle = async () => {
      // for (let key in server.selvaClient.) {
      //   const obj = server.selvaClient.redis.serversById[key]
      //   const ts = obj.stats && obj.stats.timestamp
      //   if (ts) {
      //     // also add on exit hook!
      //     if (Date.now() - ts > 5e3) {
      //       const redis = server.selvaClient.redis
      //       const id = `${obj.host}:${obj.port}`
      //       await Promise.all([
      //         redis.srem({ type: 'registry' }, 'servers', id),
      //         redis.del({ type: 'registry' }, id)
      //       ])
      // send correct event here
      //       await redis.publish(
      //         { type: 'registry' },
      //         constants.REGISTRY_UPDATE,
      //         id
      //       )
      //     }
      //   } else {
      //     // fill it if it does timeout remove
      //     // obj.stats = { timestamp: Date.now() }
      //   }
      // }
      setTimeout(cleanIdle, 1e3)
    }
    cleanIdle()
  })
}

// then fix the client destroy methods

// add marker from schema subs
