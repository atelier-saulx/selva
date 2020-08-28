import { SelvaServer } from '../'
import { constants } from '@saulx/selva'

const { REGISTRY_UPDATE } = constants

export const registryManager = (server: SelvaServer) => {
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

  const cleanIdle = async () => {
    const q = []
    for (const id of server.selvaClient.servers.ids) {
      q.push(async () => {
        const { stats, ...descriptor } = JSON.parse(
          await server.selvaClient.redis.hget(
            { type: 'registry' },
            id,
            'stats',
            'name',
            'host',
            'port',
            'type'
          )
        )

        // also going to do ordering of replicas here

        const ts = stats.timestamp
        if (Date.now() - ts > 5e3) {
          const redis = server.selvaClient.redis
          await Promise.all([
            redis.srem({ type: 'registry' }, 'servers', id),
            redis.del({ type: 'registry' }, id)
          ])
          await redis.publish(
            { type: 'registry' },
            REGISTRY_UPDATE,
            JSON.stringify({
              event: 'remove',
              server: descriptor
            })
          )
        }
      })
    }
    await Promise.all(q)
    setTimeout(cleanIdle, 1e3)
  }
  cleanIdle()
}

// then fix the client destroy methods

// add marker from schema subs
