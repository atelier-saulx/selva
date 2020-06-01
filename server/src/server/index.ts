import { ServerType, connect, SelvaClient } from '@saulx/selva'
import { ServerOptions } from '../types'
import { EventEmitter } from 'events'
import startRedis from './startRedis'
import chalk from 'chalk'
import ProcessManager from './processManager'
import attachStatusListeners from './attachStatusListeners'
import {
  startSubscriptionManager,
  stopSubscriptionManager,
  SubscriptionManagerState
} from './subscriptionManager'
import { startAsyncTaskWorker, stopAsyncTaskWorker } from './asyncTask'
import {
  BackupFns,
  saveAndBackUp,
  scheduleBackups,
  loadBackup
} from '../backups'

export class SelvaServer extends EventEmitter {
  public type: ServerType
  public port: number
  public host: string
  public registry: SelvaClient
  public pm: ProcessManager
  public subscriptionManager: SubscriptionManagerState
  private backupFns: BackupFns
  private backupDir: string
  private backupCleanup: Function

  constructor(type: ServerType) {
    super()
    this.setMaxListeners(10000)
    this.type = type
  }

  async start(opts: ServerOptions) {
    console.info(
      `Start SelvaServer ${chalk.white(opts.name)} of type ${chalk.blue(
        this.type
      )} on port ${chalk.blue(String(opts.port))}`
    )

    this.port = opts.port
    this.host = opts.host
    if (opts.backups) {
      this.backupFns = await opts.backups.backupFns
    }

    this.backupDir = opts.dir

    if (opts.registry) {
      this.registry = connect(opts.registry)

      // important to define that you want to get stuff from the registry! - do it in nested methods
      // in get and set you can also pass 'registry'
    } else if (this.type === 'registry') {
      this.registry = connect({ port: opts.port })
    }

    if (opts.backups && opts.backups.loadBackup) {
      console.log('Loading backup')
      await loadBackup(this.backupDir, this.backupFns)
      console.log('Backup loaded')
    }

    await startRedis(this, opts)

    if (this.type === 'origin' && opts.backups && opts.backups.scheduled) {
      this.backupCleanup = scheduleBackups(
        opts.dir,
        opts.backups.scheduled.intervalInMinutes,
        this.backupFns
      )
    }

    attachStatusListeners(this, opts)

    if (this.type === 'subscriptionManager') {
      this.subscriptionManager = await startSubscriptionManager(opts)
    }
  }

  async destroy() {
    if (this.pm) {
      this.pm.destroy()
      this.pm = undefined
    }
    if (this.type === 'subscriptionManager') {
      await stopSubscriptionManager(this.subscriptionManager)
    }

    if (this.backupCleanup) {
      this.backupCleanup()
      this.backupCleanup = undefined
    }

    this.emit('close')
  }

  async backup() {
    if (!this.backupFns) {
      throw new Error(`No backup options supplied`)
    }

    await saveAndBackUp(this.backupDir, this.port, this.backupFns)
  }
}

export const startServer = async (
  type: ServerType,
  opts: ServerOptions
): Promise<SelvaServer> => {
  const server = new SelvaServer(type)
  await server.start(opts)
  return server
}
