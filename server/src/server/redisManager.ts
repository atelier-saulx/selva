import ProcessManager from './processManager'
import { SelvaClient } from '@saulx/selva'

export default class RedisManager extends ProcessManager {
  private redisPort: number
  private redisHost: string
  private selvaClient: SelvaClient

  constructor(
    args: string[],
    {
      host,
      port,
      selvaClient
    }: { host: string; port: number; selvaClient: SelvaClient }
  ) {
    super('redis-server', args)

    this.redisHost = host
    this.redisPort = port
    this.selvaClient = selvaClient
  }

  protected async collect(): Promise<any> {
    const runtimeInfo = await super.collect()

    try {
      const info = await this.selvaClient.redis.info({
        port: this.redisPort,
        host: this.redisHost
      })

      if (info) {
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
      // store busy
      console.error('! cannot get info we may need to restart it!')
      return {
        redisInfo: {},
        runtimeInfo,
        err: err.message,
        isBusy: true
      }
    }
  }
}
