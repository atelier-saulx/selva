import { ConnectOptions, ServerType } from '@saulx/selva'

import { BackupFns } from './backups'

export type ServerOptions = {
  save?: boolean | { seconds: number; changes: number }
  backups?: {
    loadBackup?: boolean
    scheduled?: { intervalInMinutes: number }
    backupFns: BackupFns | Promise<BackupFns>
  }
  registry?: ConnectOptions
  port?: number
  host?: string
  name?: string
  modules?: string[]
  dir?: string
  default?: boolean
  attachToExisting?: boolean
}

export type Stats = {
  cpu: number
  activeChannels: number
  opsPerSecond: number
  timestamp: number
}

export type RegistryInfo = {
  busy?: boolean
  name?: string
  type?: ServerType
  stats?: Stats
  host: string
  port: number
}

export type Options =
  | ServerOptions
  | (() => Promise<ServerOptions>)
  | Promise<ServerOptions>
