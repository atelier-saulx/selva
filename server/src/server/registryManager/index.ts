import { SelvaServer } from '../'
import { constants } from '@saulx/selva'

export const registryManager = (server: SelvaServer): Promise<void> => {
  return new Promise(resolve => {
    // listener on registry update info
    // if info < 30 seconds unregister server
    const cleanIdle = async () => {
      for (let key in server.selvaClient.redis.serversById) {
        const obj = server.selvaClient.redis.serversById[key]
        const ts = obj.stats && obj.stats.timestamp
        if (ts) {
          // also add on exit hook!
          if (Date.now() - ts > 5e3) {
            const redis = server.selvaClient.redis
            const id = `${obj.host}:${obj.port}`
            await Promise.all([
              redis.srem({ type: 'registry' }, 'servers', id),
              redis.del({ type: 'registry' }, id)
            ])

            await redis.publish(
              { type: 'registry' },
              constants.REGISTRY_UPDATE,
              id
            )
          }
        } else {
          // fill it if it does timeout remove
          // obj.stats = { timestamp: Date.now() }
        }
      }

      setTimeout(cleanIdle, 1e3)
    }

    cleanIdle()
  })
}

// then fix the client destroy methods

// add marker from schema subs
