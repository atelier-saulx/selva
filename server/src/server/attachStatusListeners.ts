import updateRegistry from './updateRegistry'
import { ServerDescriptor } from '@saulx/selva'
import { ServerOptions, Stats } from '../types'
import { SelvaServer } from './'

const attachStatusListeners = (server: SelvaServer, opts: ServerOptions) => {
  const info: ServerDescriptor = {
    name: opts.name,
    type: server.type,
    port: opts.port,
    host: opts.host
  }
  server.on('stats', rawStats => {
    if (server.type === 'replica') {
      console.log(rawStats.redisInfo.sync_full, rawStats.redisInfo.master_sync_in_progress, rawStats.redisInfo.sync_partial_ok, rawStats.redisInfo.role, server.type)
    }
    const stats: Stats = {
      cpu: rawStats.runtimeInfo.cpu,
      activeChannels: Number(rawStats.redisInfo.pubsub_channels),
      opsPerSecond: Number(rawStats.redisInfo.instantaneous_ops_per_sec),
      timestamp: rawStats.runtimeInfo.timestamp
    }
    updateRegistry(
      server,
      Object.assign(
        {
          stats
        },
        info
      )
    )
  })
}

export default attachStatusListeners
