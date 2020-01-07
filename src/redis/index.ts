import { promisify } from 'util'
import { createClient, RedisClient as Redis, Commands } from 'redis'

export type ConnectOptions = {
  port: number
  host: string
  retryStrategy?: () => number // make a good default dont want to confiure this all the time
  // how we currently use this type is a 'service type' which can hold a bit more info, like name, id etc of the service)
}

type RedisCommand = {
  command: string
  args: string[]
  resolve: (x: any) => void
  reject: (x: Error) => void
}

export default class RedisClient {
  private connector: () => Promise<ConnectOptions>
  private client: Redis
  private buffer: RedisCommand[]

  constructor(connect: ConnectOptions | (() => Promise<ConnectOptions>)) {
    this.connector =
      typeof connect === 'object' ? () => Promise.resolve(connect) : connect
  }

  private async connect() {
    const opts = await this.connector()
    this.client = createClient(opts)
    await this.flushBuffered()
  }

  private async queue(
    command: string,
    args: string[],
    resolve: (x: any) => void,
    reject: (x: Error) => void
  ) {
    this.buffer.push({
      command,
      args,
      resolve,
      reject
    })
  }

  // FIXME: this is not ready
  private async flushBuffered() {
    for (const cmd of this.buffer) {
      await new Promise((resolve, _) => {
        this.client.sendCommand(cmd.command, cmd.args, (err, reply) => {
          if (err) {
            cmd.reject(err)
          } else {
            cmd.resolve(reply)
          }

          resolve()
        })
      })
    }
  }
}
