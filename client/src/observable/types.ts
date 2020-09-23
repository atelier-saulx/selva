import { GetOptions } from '../get'

export type GetObservableOptions = {
    type: 'get'
    cache?: boolean
    maxMemory?: number
    options: GetOptions
}

export type SchemaObservableOptions = {
    type: 'schema'
    db: string
}

export type ObservableOptions = GetObservableOptions | SchemaObservableOptions