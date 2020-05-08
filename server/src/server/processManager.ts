import { spawn, ChildProcess } from 'child_process'
import { EventEmitter } from 'events'

export default class ProcessManager extends EventEmitter {
  private command: string
  private args: string[]
  private childProcess: ChildProcess

  constructor(command: string, args: string[]) {
    super()
    this.command = command
    this.args = args
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
  }

  destroy() {
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
