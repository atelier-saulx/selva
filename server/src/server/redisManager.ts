import { resolve as pathResolve } from 'path'
import ProcessManager from './processManager'
import { SelvaClient, ServerType } from '@saulx/selva'
import { loadScripts } from './attachStatusListeners'

export default class RedisManager extends ProcessManager {
  private redisPort: number
  private redisHost: string
  private opts: {
    host: string
    port: number
    selvaClient: SelvaClient
    name: string
    type: ServerType
  }
  private selvaClient: SelvaClient
  private type: ServerType
  private name: string

  constructor(
    args: string[],
    opts: {
      host: string
      port: number
      selvaClient: SelvaClient
      name: string
      type: ServerType
    }
  ) {
    const { host, port, selvaClient, type, name } = opts

    const platform = process.platform + '_' + process.arch
    const command = `${__dirname}/../../modules/binaries/${platform}/redis-server-selva`
    super(command, {
      args,
      env:
        process.platform === 'linux'
          ? {
              REDIS_PORT: port.toString(),
              SERVER_TYPE: type,
              LD_LIBRARY_PATH: `${__dirname}/../../modules/binaries/${platform}:/usr/local/lib`,
              LOCPATH: pathResolve(
                __dirname,
                `../../modules/binaries/${platform}/locale`
              ), // MacOS libSystem will ignore this
            }
          : {
              REDIS_PORT: port.toString(),
              SERVER_TYPE: type ,
              DYLD_FALLBACK_LIBRARY_PATH: `${__dirname}/../../modules/binaries/${platform}`,
            },
    })

    this.opts = opts
    this.redisHost = host
    this.redisPort = port
    this.selvaClient = selvaClient
    this.type = type
    this.name = name
  }

  protected async collect(): Promise<any> {
    const runtimeInfo = await super.collect()

    try {
      let timeout
      const wait = () =>
        new Promise((_resolve, reject) => {
          timeout = setTimeout(
            () => reject(new Error('Info-timeout took longer then 2 seconds')),
            2e3
          )
        })

      const info = await Promise.race([
        this.selvaClient.redis.info({
          port: this.redisPort,
          host: this.redisHost,
          type: this.type,
          name: this.name,
        }),
        wait(),
      ])

      clearTimeout(timeout)

      if (info && typeof info === 'string') {
        const infoLines = info.split('\r\n')

        const redisInfo = infoLines.reduce((acc, line) => {
          if (line.startsWith('#')) {
            return acc
          }

          const [key, val] = line.split(':')
          if (key === '') {
            return acc
          }

          return {
            ...acc,
            [key]: val,
          }
        }, {})

        return { redisInfo, runtimeInfo }
      } else {
        return { isBusy: true, runtimeInfo }
      }
    } catch (err) {
      // this.emit('error', err)
      return {
        redisInfo: {},
        runtimeInfo,
        err: err.message,
        isBusy: true,
      }
    }
  }

  async initializeState() {
    return loadScripts(this.opts)
  }
}
