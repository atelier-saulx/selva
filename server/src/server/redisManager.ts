import { exec } from 'child_process'
import { promisify } from 'util'
import ProcessManager from './processManager'

export default class RedisManager extends ProcessManager {
  constructor(args: string[]) {
    super('redis-server', args)
  }

  protected async collect(): Promise<any> {
    const runtimeInfo = await super.collect()

    const info = await promisify(exec)('redis-cli INFO')
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
