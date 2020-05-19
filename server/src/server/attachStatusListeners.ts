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

  if (opts.default) {
    info.default = true
  }

  server.on('stats', rawStats => {
    const stats: Stats = {
      memory: rawStats.runtimeInfo.memory,
      redisMemory: Number(rawStats.redisInfo.used_memory),
      cpu: rawStats.runtimeInfo.cpu,
      luaMemory: Number(rawStats.redisInfo.used_memory_lua),
      totalMemoryAvailable: Number(rawStats.redisInfo.total_system_memory),
      memoryFragmentationRatio: Number(
        rawStats.redisInfo.mem_fragmentation_ratio
      ),
      lastSaveTime: Number(rawStats.redisInfo.rdb_last_save_time),
      uptime: rawStats.runtimeInfo.elapsed,
      lastSaveError:
        rawStats.redisInfo.last_bgsave_status === 'ok' ? false : true,
      totalNetInputBytes: Number(rawStats.redisInfo.total_net_input_bytes),
      totalNetOutputBytes: Number(rawStats.redisInfo.total_net_output_bytes),
      activeChannels: Number(rawStats.redisInfo.pubsub_channels),
      opsPerSecond: Number(rawStats.redisInfo.instantaneous_ops_per_sec),
      timestamp: rawStats.runtimeInfo.timestamp
    }

    updateRegistry(
      server.registry,
      Object.assign(
        {
          stats
        },
        info
      )
    )
  })

  server.on('busy', () => {})

  server.on('subscription', () => {})

  console.log('Registering server', info)
  updateRegistry(server.registry, info)
}

export default attachStatusListeners
