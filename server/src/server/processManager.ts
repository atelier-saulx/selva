import { spawn, ChildProcess } from 'child_process'
import pidusage, { Status } from 'pidusage'
import { EventEmitter } from 'events'

// const LOAD_MEASUREMENTS_INTERVAL = 60 * 1e3 // every minute
const LOAD_MEASUREMENTS_INTERVAL = 5e3 // every 10 seconds

export default class ProcessManager extends EventEmitter {
  private command: string
  private args: string[]
  private childProcess: ChildProcess
  private loadMeasurementsTimeout: NodeJS.Timeout

  constructor(command: string, args: string[]) {
    super()
    this.command = command
    this.args = args
  }

  protected async collect(): Promise<any> {
    if (this.childProcess && this.childProcess.pid) {
      return await pidusage(this.childProcess.pid)
    }
  }

  private startLoadMeasurements() {
    let isFirst = true
    this.loadMeasurementsTimeout = setTimeout(
      () => {
        this.collect()
          .then(data => {
            isFirst = false
            this.emit('stats', data)
          })
          .catch(e => {
            console.error(
              `Error collecting load measurements from ${this.command}`,
              e
            )
          })
          .finally(() => {
            this.startLoadMeasurements()
          })
      },
      isFirst ? 0 : LOAD_MEASUREMENTS_INTERVAL
    )
  }

  private stopLoadMeasurements() {
    if (this.loadMeasurementsTimeout) {
      clearTimeout(this.loadMeasurementsTimeout)
      this.loadMeasurementsTimeout = undefined
    }
  }

  start() {
    if (this.childProcess) {
      return
    }

    this.childProcess = spawn(this.command, this.args)

    this.childProcess.stdout.on('data', d => {
      this.emit('stdout', d.toString())
    })

    this.childProcess.stderr.on('data', d => {
      this.emit('stderr', d.toString())
    })

    const exitHandler = (code: number) => {
      console.log(
        `Child process for ${this.command} exited with code ${code}. Restarting...`
      )

      this.childProcess.removeAllListeners()
      this.childProcess = undefined

      this.start()
    }

    this.childProcess.on('exit', exitHandler)
    this.childProcess.on('close', exitHandler)

    this.startLoadMeasurements()
  }

  destroy() {
    this.stopLoadMeasurements()

    if (this.childProcess) {
      this.childProcess.removeAllListeners()
      this.removeAllListeners() // yesh?
      const cp = this.childProcess
      this.childProcess = undefined
      cp.kill('SIGTERM')
      setTimeout(() => {
        const ok = cp.kill('SIGKILL')
        if (ok) {
          console.log(
            `Child process for ${this.command} didn't terminate within 10 seconds. Sending SIGKILL.`
          )
        }
      }, 1000 * 10)
    }
  }
}

if (module === require.main) {
  // TODO: remove test stuff
  const pm = new ProcessManager('redis-server', [
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
