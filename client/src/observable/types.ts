import { GetOptions } from '../get'

export type GetObservableOptions = {
  type: 'get'
  cache?: boolean
  maxMemory?: number
  options: GetOptions
  immutable?: boolean
  raw?: boolean
}

export type ObsSettings = {
  cache?: boolean
  maxMemory?: number
  immutable?: boolean
  raw?: boolean
}

export type SchemaObservableOptions = {
  type: 'schema'
  db: string
  cache?: boolean
  maxMemory?: number
  immutable?: boolean
  raw?: boolean
}

export type ObservableOptions = GetObservableOptions | SchemaObservableOptions
