import {
  ServerType,
  connect,
  SelvaClient,
  ServerDescriptor
} from '@saulx/selva'
import { ServerOptions } from '../types'
import { EventEmitter } from 'events'
import startRedis from './startRedis'
import ProcessManager from './processManager'
import attachStatusListeners from './attachStatusListeners'
import {
  startSubscriptionManager,
  stopSubscriptionManager,
  SubscriptionManagerState
} from './subscriptionManager'
import {
  BackupFns,
  saveAndBackUp,
  scheduleBackups,
  loadBackup
} from '../backups'
import { registryManager } from './registryManager'
import heartbeat from './heartbeat'

export class SelvaServer extends EventEmitter {
  public type: ServerType
  public port: number
  public host: string
  public name: string
  public selvaClient: SelvaClient
  public serverHeartbeatTimeout?: NodeJS.Timeout
  public pm: ProcessManager
  public origin: ServerDescriptor
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
    this.port = opts.port
    this.host = opts.host
    this.name = opts.name

    if (opts.backups && opts.backups.backupFns) {
      if (!opts.save && opts.save !== false) {
        opts.save = true
      }

      this.backupFns = await opts.backups.backupFns
    }

    this.backupDir = opts.dir

    if (opts.registry) {
      this.selvaClient = connect(opts.registry)

      // important to define that you want to get stuff from the registry! - do it in nested methods
      // in get and set you can also pass 'registry'
    } else if (this.type === 'registry') {
      this.selvaClient = connect({ port: opts.port })
    }

    this.selvaClient.server = {
      name: opts.name,
      port: opts.port,
      host: opts.host,
      type: this.type
    }

    if (opts.backups && opts.backups.loadBackup) {
      await loadBackup(this.backupDir, this.backupFns)
    }

    if (this.type === 'replica') {
      const initReplica = async () => {
        const origin = await this.selvaClient.getServer({
          name: opts.name,
          type: 'origin'
        })
        if (!this.origin) {
          this.origin = origin
          startRedis(this, opts)
        } else if (
          origin.port !== this.origin.port ||
          origin.host !== this.origin.host
        ) {
          this.pm.destroy()
          this.origin = origin
          setTimeout(() => {
            startRedis(this, opts)
          }, 500)
        }
      }
      this.selvaClient.on('added-servers', initReplica)
      this.selvaClient.on('removed-servers', initReplica)
      initReplica()
    } else {
      startRedis(this, opts)
    }

    if (this.type === 'origin' && opts.backups && opts.backups.scheduled) {
      this.backupCleanup = scheduleBackups(
        opts.dir,
        opts.backups.scheduled.intervalInMinutes,
        this.backupFns
      )
    }

    attachStatusListeners(this, opts)

    if (this.type !== 'replica') {
      heartbeat(this)
    }

    if (this.type === 'subscriptionManager') {
      this.subscriptionManager = await startSubscriptionManager(opts)
    } else if (this.type === 'registry') {
      registryManager(this)
    }
  }

  async destroy() {
    if (this.pm) {
      console.log('DESTROY SERVER')
      this.pm.destroy()
      this.pm = undefined
    }
    if (this.type === 'subscriptionManager') {
      await stopSubscriptionManager(this.subscriptionManager)
    }

    // need to call destroy if it crashes
    clearTimeout(this.serverHeartbeatTimeout)

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
