import {
  ServerType,
  connect,
  SelvaClient,
  ServerDescriptor
} from '@saulx/selva'
import { ServerOptions, Stats } from '../types'
import { EventEmitter } from 'events'
import startRedis from './startRedis'
import chalk from 'chalk'
import updateRegistry from './updateRegistry'
import ProcessManager from './processManager'

export class SelvaServer extends EventEmitter {
  public type: ServerType
  public port: number
  public host: string
  public registry: SelvaClient
  public pm: ProcessManager

  constructor(type: ServerType) {
    super()
    this.setMaxListeners(10000)
    this.type = type
  }

  attachStatusListeners(opts: ServerOptions) {
    const info: ServerDescriptor = {
      name: opts.name,
      type: this.type,
      port: opts.port,
      host: opts.host
    }

    if (opts.default) {
      info.default = true
    }

    this.on('stats', rawStats => {
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
        this.registry,
        Object.assign(
          {
            stats
          },
          info
        )
      )
    })

    this.on('busy', () => {})

    this.on('subscription', () => {})

    updateRegistry(this.registry, info)
  }

  start(opts: ServerOptions) {
    console.info(
      `Start SelvaServer ${chalk.white(opts.name)} of type ${chalk.blue(
        this.type
      )} on port ${chalk.blue(String(opts.port))}`
    )

    this.port = opts.port
    this.host = opts.host

    startRedis(this, opts)

    if (opts.registry) {
      console.log('create registry client on the server')
      this.registry = connect(opts.registry)
      // important to define that you want to get stuff from the registry! - do it in nested methods
      // in get and set you can also pass 'registry'
    } else if (this.type === 'registry') {
      console.log('im the registry - register myself')
      this.registry = connect({ port: opts.port })
    }

    this.attachStatusListeners(opts)
    // after this check what type you are
    // check if opts.registry
    // handle monitoring to registry
  }

  destroy() {
    if (this.pm) {
      this.pm.destroy()
      this.pm = undefined
    }
  }
}

export const startServer = (
  type: ServerType,
  opts: ServerOptions
): SelvaServer => {
  const server = new SelvaServer(type)
  server.start(opts)
  return server
}
