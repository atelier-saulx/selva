import ProcessManager from './processManager'
import { SelvaClient, ServerType } from '@saulx/selva'
import { createClient } from 'redis'

export default class RedisManager extends ProcessManager {
  private redisPort: number
  private redisHost: string
  private selvaClient: SelvaClient
  private type: ServerType
  private name: string

  constructor(
    args: string[],
    {
      host,
      port,
      selvaClient,
      type,
      name
    }: {
      host: string
      port: number
      selvaClient: SelvaClient
      name: string
      type: ServerType
    }
  ) {
    super('redis-server', args)

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
          name: this.name
        }),
        wait()
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
            [key]: val
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
        isBusy: true
      }
    }
  }
}
