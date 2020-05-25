import { exec } from 'child_process'
import { promisify } from 'util'
import ProcessManager from './processManager'

export default class RedisManager extends ProcessManager {
  private redisPort: number

  constructor(args: string[]) {
    super('redis-server', args)
    this.redisPort = Number(args[args.indexOf('--port') + 1])
  }

  protected async collect(): Promise<any> {
    const runtimeInfo = await super.collect()

    try {
      const info = await promisify(exec)(`redis-cli -p ${this.redisPort} INFO`)
      const infoLines = info.stdout.split('\r\n')
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
    } catch (err) {
      return { redisInfo: {}, runtimeInfo, shutdown: true }
    }
  }
}

if (module === require.main) {
  // TODO: remove test stuff
  const pm = new RedisManager([
    '--loadmodule',
    './modules/binaries/darwin_x64/redisearch.so',
    '--loadmodule',
    './modules/binaries/darwin_x64/selva.so'
  ])

  pm.on('stdout', console.log)
  pm.on('stats', console.log)
  pm.on('stderr', console.error)

  pm.start()

  setTimeout(() => {
    console.log('Closing...')
    pm.destroy()
    process.exit(0)
  }, 5e3)
}
