import { QueryResult } from 'pg'
import { FieldSchema } from '../schema'
import { SelvaClient } from '..'
import PGConnection from '../connection/pg'
import TimeseriesCache from './timeseriesCache'

export type TimeseriesContext = {
  operation: 'select' | 'insert' | 'create'
  nodeType: string
  field: string
  fieldSchema?: FieldSchema
  startTime?: number
  endTime?: number
}

export class TimeseriesClient {
  private client: SelvaClient
  public pg: PGConnection
  public tsCache: TimeseriesCache

  private isConnected: boolean = false

  constructor(client: SelvaClient) {
    // TODO: credentials
    this.client = client
    this.pg = new PGConnection({ user: 'postgres', password: 'baratta' })
    this.tsCache = new TimeseriesCache(this.client)
  }

  async connect() {
    if (this.isConnected) {
      return
    }

    await this.tsCache.subscribe()
    this.isConnected = true
  }

  disconnect() {
    if (!this.isConnected) {
      return
    }

    this.tsCache.unsubscribe()
    this.isConnected = false
  }

  private executeCreate<T>(
    _selector: TimeseriesContext,
    query: string,
    params: unknown[]
  ) {
    const instances = Object.keys(this.tsCache.instances)
    if (!instances.length) {
      throw new Error(`No instance available to run query ${query}`)
    }

    let minId = instances[0]
    let minVal = this.tsCache.instances[instances[0]].meta
      .totalRelationSizeBytes
    for (let i = 1; i < instances.length; i++) {
      const id = instances[i]
      const { meta } = this.tsCache.instances[id]

      if (meta.totalRelationSizeBytes < minVal) {
        minId = id
        minVal = meta.totalRelationSizeBytes
      }
    }

    return this.pg.execute<T>(minId, query, params)
  }

  public async execute<T>(
    selector: TimeseriesContext,
    query: string,
    params: unknown[]
  ): Promise<QueryResult<T>> {
    if (selector.operation === 'create') {
      return this.executeCreate<T>(selector, query, params)
    }

    // TODO: real logic for selecting shard
    const tsName = `${selector.nodeType}$${selector.field}`
    const shards = this.tsCache.index[tsName]
    if (!shards || !shards[0]) {
      // TODO: implicitly create? or error and create it in the catch?
      throw new Error(`Timeseries ${tsName} does not exist`)
    }

    const pgInstance = shards[0].descriptor
    return this.pg.execute(pgInstance, query, params)
  }
}
