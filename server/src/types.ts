import { ConnectOptions, ServerType } from '@saulx/selva'

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
  modules?: string[]
  dir?: string
  default?: boolean
  attachToExisting?: boolean
}

export type Stats = {
  memory: number
  redisMemory: number
  cpu: number
  uptime: number
  luaMemory: number
  // total mem on the machine redis is running on
  totalMemoryAvailable: number
  // if its above 1.5 restart server - above 1 allrdy fishy
  memoryFragmentationRatio: number
  lastSaveTime: number
  // true is bad false is good
  lastSaveError: boolean
  totalNetInputBytes: number
  totalNetOutputBytes: number
  activeChannels: number
  opsPerSecond: number
  timestamp: number
}

export type RegistryInfo = {
  busy?: boolean
  name?: string
  type?: ServerType
  stats?: Stats
  default?: boolean
  host: string
  port: number
}

export type Options =
  | ServerOptions
  | (() => Promise<ServerOptions>)
  | Promise<ServerOptions>
