import { spawn, ChildProcess } from 'child_process'
import pidusage from 'pidusage'
import { EventEmitter } from 'events'
import chalk from 'chalk'

// const LOAD_MEASUREMENTS_INTERVAL = 60 * 1e3 // every minute
const LOAD_MEASUREMENTS_INTERVAL = 1e3 // every 10 seconds
let cnt = 0

export default class ProcessManager extends EventEmitter {
  private command: string
  private args: string[]
  private env: Record<string, string>
  private childProcess: ChildProcess
  private loadMeasurementsTimeout: NodeJS.Timeout

  private restartTimer: NodeJS.Timeout

  private isMeasuring: boolean

  public uuid: number

  public restartCount: number = 0
  public isDestroyed: boolean
  public pid: number

  public errorLog: string[]

  public successTimeout: NodeJS.Timeout

  constructor(
    command: string,
    opts?: { args: string[]; env?: Record<string, string> }
  ) {
    super()
    this.command = command
    this.env = opts.env || {}
    this.args = opts.args
    this.uuid = ++cnt

    this.errorLog = []

    this.on('error', () => {})
    this.on('stderr', (data) => {
      this.errorLog.unshift(data)
      if (this.errorLog.length === 30) {
        this.errorLog.pop()
      }
    })
    this.on('stdout', (data) => {
      this.errorLog.unshift(data)
      if (this.errorLog.length === 30) {
        this.errorLog.pop()
      }
    })
  }

  protected async collect(): Promise<any> {
    if (this.childProcess && this.childProcess.pid) {
      return pidusage(this.childProcess.pid)
    }
  }

  private startLoadMeasurements(isNotFirst: boolean = false) {
    this.isMeasuring = true

    this.loadMeasurementsTimeout = setTimeout(
      () => {
        this.collect()
          .then((data) => {
            if (data.isBusy) {
              this.emit('busy', data)
            } else {
              this.emit('stats', data)
            }
          })
          .catch((e) => {
            // console.error(
            //   `Error collecting load measurements from ${this.command}`,
            //   e
            // )
          })
          .finally(() => {
            if (this.isMeasuring) {
              this.startLoadMeasurements(true)
            }
          })
      },
      isNotFirst ? LOAD_MEASUREMENTS_INTERVAL : 0
    )
  }

  private stopLoadMeasurements() {
    this.isMeasuring = false
    if (this.loadMeasurementsTimeout) {
      clearTimeout(this.loadMeasurementsTimeout)
      this.loadMeasurementsTimeout = undefined
    }
  }

  start() {
    if (this.childProcess) {
      return
    }

    this.childProcess = spawn(this.command, this.args, {
      env: { ...process.env, ...this.env },
    })

    this.pid = this.childProcess.pid

    this.childProcess.stdout.on('data', (d) => {
      this.emit('stdout', d.toString())
    })

    this.childProcess.stderr.on('data', (d) => {
      this.emit('stderr', d.toString())
    })

    const exitHandler = (code: number, signal: string) => {
      this.emit(
        'error',
        new Error(`Child process for ${this.command} exited with code ${code}.`)
      )

      this.childProcess.removeAllListeners()
      this.childProcess = undefined

      clearTimeout(this.restartTimer)
      clearTimeout(this.successTimeout)

      this.restartTimer = setTimeout(() => {
        console.error(
          chalk.red(
            `Child process for ${this.command} exited with ${
              code === null ? `signal ${signal}` : `code ${code}`
            } at ${new Date().toLocaleTimeString()} ${new Date().toLocaleDateString()} pm: ${
              this.uuid
            } port: ${this.args[1]}. Attempting restart #${
              this.restartCount + 1
            }`
          )
        )

        clearTimeout(this.successTimeout)

        this.restartCount++
        if (this.restartCount > 4) {
          console.info('')
          const err = new Error(`Tried restarting server 5 times`)

          for (let i = this.errorLog.length - 1; i > -1; i--) {
            console.info(chalk.grey(`Redis log #${i}:`, this.errorLog[i]))
          }
          err.stack = this.errorLog.join('\n')

          console.info(
            chalk.red(
              `Tried restarting server 5 times something is wrong pm: ${
                this.uuid
              } port: ${
                this.args[1]
              } at ${new Date().toLocaleTimeString()} ${new Date().toLocaleDateString()}`
            )
          )
          console.info(chalk.grey(`${this.command} ${this.args}`))
          console.info('')
          throw err
        } else {
          this.start()

          this.successTimeout = setTimeout(() => {
            this.restartCount = 0
            console.info('')

            for (let i = this.errorLog.length - 1; i > -1; i--) {
              console.info(chalk.grey(`Redis log #${i}:`, this.errorLog[i]))
            }

            console.info(
              chalk.green(
                `Restarted server successfully after crash pm: ${this.uuid} port: ${this.args[1]}`
              )
            )
            console.info('')
          }, 2e3)
        }
      }, 1000)
    }

    this.childProcess.on('exit', exitHandler)
    this.childProcess.on('close', exitHandler)

    this.startLoadMeasurements()
  }

  destroy(signal?: NodeJS.Signals) {
    this.isDestroyed = true
    this.stopLoadMeasurements()

    if (this.childProcess) {
      this.childProcess.removeAllListeners()
      this.removeAllListeners() // yesh?
      const cp = this.childProcess
      this.childProcess = undefined
      cp.kill(signal || 'SIGTERM')
      setTimeout(() => {
        const ok = cp.kill('SIGKILL')
        if (ok) {
          console.info(
            `Child process for ${this.command} didn't terminate within 10 seconds. Sending SIGKILL.`
          )
        }
      }, 1000 * 10)
    }
  }
}
