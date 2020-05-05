import { ConnectOptions } from '@saulx/selva'

import { BackupFns } from './backups'

export type ServerOptions = {
  backups?: {
    loadBackup?: boolean
    scheduled?: { intervalInMinutes: number }
    backupFns: BackupFns | Promise<BackupFns>
  }
  registry?: ConnectOptions
  port?: number
  host?: string
  name?: string
  replica?: string
  modules?: string[]
  dir?: string
  default?: boolean
}

export type Options =
  | ServerOptions
  | (() => Promise<ServerOptions>)
  | Promise<ServerOptions>
