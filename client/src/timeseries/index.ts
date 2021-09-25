import { QueryResult } from 'pg'
import { FieldSchema } from '../schema'
import { SelvaClient } from '..'
import PGConnection from '../connection/pg'
import TimeseriesCache from './timeseriesCache'
import { PG } from '../connection/pg/client'

export type TimeseriesContext = {
  nodeType: string
  field: string
  fieldSchema?: FieldSchema
  startTime?: number
  endTime?: number
  order?: 'asc' | 'desc'
  limit?: number
  offset?: number
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
    // TODO disconnect automatically after a while?
    if (!this.isConnected) {
      return
    }

    this.tsCache.unsubscribe()
    this.isConnected = false
  }

  public getMinInstance(): PG {
    const instances = Object.keys(this.tsCache.instances)
    if (!instances.length) {
      return null
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

    return this.pg.getClient(minId)
  }

  public hasTimeseries(selector: TimeseriesContext): boolean {
    const tsName = `${selector.nodeType}$${selector.field}`
    return !!this.tsCache.index[tsName]
  }

  // TODO: the query here needs to be a higher level consruct than SQL, because we need to adjust query contents based on shard targeted
  public async execute<T>(
    selector: TimeseriesContext,
    query: string,
    params: unknown[]
  ): Promise<QueryResult<T>> {
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
