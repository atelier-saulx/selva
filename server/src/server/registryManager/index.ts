import { SelvaServer } from '../'
import { constants, SelvaClient } from '@saulx/selva'
import { integer } from 'aws-sdk/clients/cloudfront'
import { Interval } from 'aws-sdk/clients/dlm'

const { REGISTRY_UPDATE } = constants

type ServerIndex = {
  index: integer
  weight: number
  id: string
  name: string
}

const insert = (array: ServerIndex[], target: ServerIndex): void => {
  var l: number = 0
  var h: number = array.length - 1
  var m: number
  while (l <= h) {
    m = (l + h) >>> 1
    const a = array[m].weight
    const b = target.weight
    if (a < b) {
      l = m + 1
    } else if (a > b) {
      h = m - 1
    } else {
      l = m
      break
    }
  }
  array.splice(l, 0, target)
}

export const registryManager = (server: SelvaServer) => {
  // not reallty nessecary but nice to see for now
  server.selvaClient.on('added-servers', ({ event, server }) => {
    // this means we are going to re-index
    if (event === '*') {
      // got all of them
      // console.log('initial servers')
    } else {
      console.log(
        'individual server is added to registry',
        server.name,
        server.type
      )
    }
  })

  server.selvaClient.on('removed-servers', ({ event, server }) => {
    if (event === '*') {
      // got all of them
      // console.log('remove all servers')
    } else {
      console.log(
        'individual server is removed from registry',
        server.name,
        server.type
      )
    }
  })

  const updateFromStats = async () => {
    const replicas: ServerIndex[] = []
    const subsManagers: ServerIndex[] = []
    const redis = server.selvaClient.redis

    await Promise.all(
      [...server.selvaClient.servers.ids].map(async id => {
        try {
          const result = await redis.hmget(
            { type: 'registry' },
            id,
            'stats',
            'name',
            'host',
            'port',
            'type',
            'index'
          )

          if (result) {
            const [rawStats, name, host, port, type, index] = result
            const stats = rawStats && JSON.parse(rawStats)

            if (!stats) {
              // not very strange this can happen on register before info update
              console.warn(
                '⚠️ ',
                type,
                name,
                id,
                'does not have stats (from registry server)'
              )
              return
            }

            const ts = stats.timestamp

            // very sensitive...
            if (Date.now() - ts > 3e3) {
              await Promise.all([
                redis.srem({ type: 'registry' }, 'servers', id),
                redis.del({ type: 'registry' }, id)
              ])

              // store it when this happens and use it as a 'ramp up' metric
              // have to store this - metric will go combined with busy status
              console.log('TIMEOUT', id, type, name)

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
            } else if (type === 'replica') {
              let weight = Math.round(stats.cpu / 5)

              // slow connection so something must be up
              if (Date.now() - ts > 2e3) {
                console.warn(
                  'Connection is slow something must be weird (emulate a weight of 100)',
                  type,
                  id
                )
                weight = 100
              }
              // opsPerSecond is also very good as a measure

              console.log(type, id, weight, 'cpu', stats.cpu, '%')

              const target: ServerIndex = {
                weight,
                id,
                name,
                index: index === null ? -1 : Number(index) // original index
              }
              insert(replicas, target)
            }
            // else subs manager (also just order them)
          }
        } catch (err) {
          console.error('Error getting from servers in registry', err, id)
        }
      })
    )

    let move
    let q
    for (let i = 0; i < replicas.length; i++) {
      const replica = replicas[i]
      if (i !== replica.index) {
        if (
          !replicas[replica.index] ||
          replica.weight !== replicas[replica.index].weight
        ) {
          if (!q) {
            q = []
            move = {}
          }
          q.push(redis.hset({ type: 'registry' }, replica.id, 'index', i))
          move[replica.id] = [i]
          if (replica.name !== 'default') {
            move[replica.id].push(replica.name)
          }
        }
      }
    }

    if (move) {
      q.push(
        redis.publish(
          {
            type: 'registry'
          },
          REGISTRY_UPDATE,
          JSON.stringify({
            event: 'update-index',
            type: 'replica',
            move
          })
        )
      )
    }

    if (q) {
      await Promise.all(q)
    }

    setTimeout(updateFromStats, 1e3)
  }
  updateFromStats()
}
