import updateRegistry from './updateRegistry'
import { SelvaClient, ServerDescriptor, ServerType } from '@saulx/selva'
import { ServerOptions, Stats } from '../types'
import { SelvaServer } from './'

import { SCRIPTS } from './scripts'

export const loadScripts = async (opts: {
  host: string
  port: number
  selvaClient: SelvaClient
  name: string
  type: ServerType
}) => {
  const info: ServerDescriptor = {
    name: opts.name,
    type: opts.type,
    port: opts.port,
    host: opts.host,
  }

  return opts.selvaClient.redis.script(
    info,
    'LOAD',
    SCRIPTS['update-schema'].content
  )
}

const initHierarchy = (
  server: SelvaServer,
  info: ServerDescriptor
): Promise<void> => {
  if (info.type === 'origin') {
    console.log('Trying to initialize empty hierarchy', info)
    return server.selvaClient.redis
      .selva_modify(info, 'root', 'R', '0', 'type', 'root')
      .then((res) => {
        console.log('Empty hierarchy initialized', res)
        return server.selvaClient.redis.script(
          info,
          'LOAD',
          SCRIPTS['update-schema'].content
        )
      })
  }

  return Promise.resolve()
}

const attachListener = (server: SelvaServer, info: ServerDescriptor) => {
  server.on('stats', (rawStats) => {
    // if (server.type === 'replica') {
    // only want this if it is not registred before
    // if (rawStats.redisInfo.master_sync_in_progress !== '0') {
    // return
    // }
    // }

    if (rawStats.runtimeInfo) {
      const stats: Stats = {
        cpu: rawStats.runtimeInfo.cpu,
        activeChannels: Number(rawStats.redisInfo.pubsub_channels),
        opsPerSecond: Number(rawStats.redisInfo.instantaneous_ops_per_sec),
        timestamp: rawStats.runtimeInfo.timestamp,
      }

      updateRegistry(
        server,
        Object.assign(
          {
            stats,
          },
          info
        )
      )
    }
  })
}

const attachStatusListeners = (server: SelvaServer, opts: ServerOptions) => {
  const info: ServerDescriptor = {
    name: opts.name,
    type: server.type,
    port: opts.port,
    host: opts.host,
  }

  initHierarchy(server, info)
    .catch((e) => console.error('ERROR initializing hierarchy', e))
    .finally(() => {
      console.log('Hierarchy initialized and lua scripts loaded')
      attachListener(server, info)
    })
}

export default attachStatusListeners
