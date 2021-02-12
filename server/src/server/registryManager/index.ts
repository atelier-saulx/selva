import { SelvaServer } from '../'
import { constants, ServerDescriptor, SelvaClient } from '@saulx/selva'
import chalk from 'chalk'

const { REGISTRY_UPDATE, REGISTRY_SUBSCRIPTION_INDEX } = constants

type ServerIndex = {
  index: number
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

const orderServers = (
  type: string,
  servers: ServerIndex[],
  client: SelvaClient,
  includeName: boolean
) => {
  const redis = client.redis
  let move
  let q
  for (let i = 0; i < servers.length; i++) {
    const server = servers[i]
    if (i !== server.index) {
      if (
        !servers[server.index] ||
        server.weight !== servers[server.index].weight
      ) {
        if (!q) {
          q = []
          move = {}
        }
        q.push(redis.hset({ type: 'registry' }, server.id, 'index', i))
        move[server.id] = [i]
        if (includeName && server.name !== 'default') {
          move[server.id].push(server.name)
        }
      }
    }
  }

  if (move) {
    q.push(
      redis.publish(
        {
          type: 'registry',
        },
        REGISTRY_UPDATE,
        JSON.stringify({
          event: 'update-index',
          type,
          move,
        })
      )
    )
  }

  return q
}

const removeServerFromSubsRegistry = async (
  client: SelvaClient,
  server: ServerDescriptor
) => {
  const id = `${server.host}:${server.port}`
  const serverIndex = await client.redis.smembers(
    { type: 'subscriptionRegistry' },
    REGISTRY_SUBSCRIPTION_INDEX + id
  )
  await Promise.all(
    serverIndex.map(async (channel: string) => {
      const prev = await client.redis.get(
        { type: 'subscriptionRegistry' },
        channel
      )
      if (prev === id) {
        await client.redis.del({ type: 'subscriptionRegistry' }, channel)
      } else {
        console.warn(
          chalk.yellow(
            `Trying to remove a subscription registry channel index that does not match. index: ${channel}:${prev} server: ${id}`
          )
        )
      }
    })
  )
  await client.redis.del(
    { type: 'subscriptionRegistry' },
    REGISTRY_SUBSCRIPTION_INDEX + id
  )
}

export const registryManager = (server: SelvaServer) => {
  // not reallty nessecary but nice to see for now
  const client = server.selvaClient

  client.on('added-servers', ({ event, server }) => {
    // this means we are going to re-index
    if (event === '*') {
      // got all of them
      // console.log('initial servers')
    } else {
      console.log(
        chalk.green('Server is added to registry'),
        server.name,
        server.type,
        server.host,
        server.port
      )
    }
  })

  client.on(
    'removed-servers',
    ({ event, server }: { event: string; server: ServerDescriptor }) => {
      if (event === '*') {
        // got all of them
        // console.log('remove all servers')
      } else {
        console.log(
          chalk.red('Server is removed from registry'),
          server.name,
          server.type,
          server.host,
          server.port
        )
        if (server.type === 'subscriptionManager') {
          removeServerFromSubsRegistry(client, server).then(() => {
            console.log(
              chalk.gray(
                'Succesfully removed subsmanager from subscription-registry'
              )
            )
          })
        }
      }
    }
  )

  const serverTimeouts: {
    [id: string]: number[]
  } = {}

  const updateFromStats = async () => {
    const replicas: ServerIndex[] = []
    const subsManagers: ServerIndex[] = []
    const redis = server.selvaClient.redis

    if (server.isDestroyed === true) {
      return
    }

    await Promise.all(
      [...server.selvaClient.servers.ids].map(async (id) => {
        try {
          const result = await redis.hmget(
            { type: 'registry' },
            id,
            'stats',
            'name',
            'host',
            'port',
            'type',
            'index',
            'subs'
          )

          if (result) {
            const [rawStats, name, host, port, type, index, subs] = result
            const stats = rawStats && JSON.parse(rawStats)

            if (!stats) {
              // not very strange this can happen on register before info update
              console.warn(
                chalk.yellow(
                  `⚠️  ${type}, ${name}, ${id} Does not have stats (from registry server)`
                )
              )
              await redis.hset(
                { type: 'registry' },
                id,
                'stats',
                JSON.stringify({
                  timestamp: Date.now(),
                })
              )
              return
            }

            const ts = stats.timestamp

            const now = Date.now()
            // very sensitive...
            if (now - ts > 8e3) {
              await Promise.all([
                redis.srem({ type: 'registry' }, 'servers', id),
                redis.del({ type: 'registry' }, id),
              ])
              console.warn(
                chalk.red(
                  `Server timed out last heartbeat ${
                    Date.now() - ts
                  }ms ago ${id}, ${type}, ${name}`
                )
              )
              // also store this - somewhere can be just in mem
              if (!serverTimeouts[id]) {
                serverTimeouts[id] = []
              }
              serverTimeouts[id].unshift(now)
              if (serverTimeouts[id].length > 50) {
                serverTimeouts[id].pop()
              }
              for (let i = 0; i < serverTimeouts[id].length; i++) {
                const timeout = serverTimeouts[id][i]
                // keep max for 1 hour
                // make this configurable for testing
                if (now - timeout > 1e3 * 60 * 60 * 1) {
                  serverTimeouts[id] = serverTimeouts[id].slice(0, i)
                  break
                }
              }
              server.emit('server-timeout', {
                id,
                serverTimeouts: serverTimeouts[id],
                port,
                host,
                name,
                type,
                index,
              })
              // ok you want to store last timeoud event maybe an array (max 10)
              // this is the metric we are going to use to
              // ramp up
              // scale
              await redis.publish(
                { type: 'registry' },
                REGISTRY_UPDATE,
                JSON.stringify({
                  event: 'remove',
                  server: {
                    name,
                    host,
                    port,
                    type,
                  },
                })
              )
            } else if (type === 'replica') {
              let weight = Math.round(stats.cpu / 5)
              // slow connection so something must be up
              if (stats.cpu === undefined) {
                console.warn(
                  chalk.yellow(
                    `Connection to replica has no full stats, emulate a weight of 100 ${type} ${id}`
                  )
                )
                weight = 100
              } else if (Date.now() - ts > 5e3 || stats.cpu === undefined) {
                console.warn(
                  chalk.yellow(
                    `Connection to replica is slow ${
                      Date.now() - ts
                    }ms since last timestamp, emulate a weight of 100 ${type} ${id}`
                  )
                )
                weight = 100
              }
              // opsPerSecond is also very good as a measure
              // console.log(type, id, weight, 'cpu', stats.cpu, '%')
              const target: ServerIndex = {
                weight,
                id,
                name,
                index: index === null ? -1 : Number(index), // original index
              }
              insert(replicas, target)
            } else if (type === 'subscriptionManager') {
              const target: ServerIndex = {
                weight: Number(subs),
                id,
                name,
                index: index === null ? -1 : Number(index), // original index
              }
              insert(subsManagers, target)
            }
            // else subs manager (also just order them)
          }
        } catch (err) {
          console.error('Error getting from servers in registry', err, id)
        }
      })
    )

    const rQ = orderServers('replica', replicas, server.selvaClient, true)
    const sQ = orderServers(
      'subscriptionManager',
      subsManagers,
      server.selvaClient,
      false
    )
    if (rQ || sQ) {
      await Promise.all(rQ && sQ ? [...rQ, ...sQ] : rQ || sQ)
    }
    server.registryTimer = setTimeout(updateFromStats, 1e3)
  }
  updateFromStats()
}
