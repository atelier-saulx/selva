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

  const updateFromStats = async () => {
    console.log('cleaning time')
    await Promise.all(
      [...server.selvaClient.servers.ids].map(async id => {
        const redis = server.selvaClient.redis
        try {
          const result = await redis.hmget(
            { type: 'registry' },
            id,
            'stats',
            'name',
            'host',
            'port',
            'type'
          )

          if (result) {
            const [rawStats, name, host, port, type] = result
            const stats = rawStats && JSON.parse(rawStats)
            if (!stats) {
              return
            }
            const ts = stats.timestamp

            console.log('???', id, type, ts, Date.now() - ts > 5e3)

            if (Date.now() - ts > 5e3) {
              await Promise.all([
                redis.srem({ type: 'registry' }, 'servers', id),
                redis.del({ type: 'registry' }, id)
              ])
              await redis.publish(
                { type: 'registry' },
                REGISTRY_UPDATE,
                JSON.stringify({
                  event: 'remove',
                  server: {
                    name,
                    host,
                    port,
                    type
                  }
                })
              )
            }
          }
        } catch (err) {
          console.error('Error getting from servers in registry', err, id)
        }
      })
    )

    setTimeout(updateFromStats, 1e3)
  }
  updateFromStats()
}
