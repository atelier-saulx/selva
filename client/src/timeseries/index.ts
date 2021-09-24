import { QueryResult } from 'pg'
import { SelvaClient } from '..'
import PGConnection from '../connection/pg'
import TimeseriesCache from './timeseriesCache'

export type TimeseriesContext = {
  nodeType: string
  field: string
  startTime?: number
  endTime?: number
}

export class TimeseriesClient {
  private client: SelvaClient
  private pg: PGConnection
  private tsCache: TimeseriesCache

  private isConnected: boolean = false

  constructor(client: SelvaClient) {
    // TODO: credentials
    this.client = client
    this.pg = new PGConnection({ user: 'postgres', password: 'baratta' })
    this.tsCache = new TimeseriesCache(client)
  }

  async connect() {
    if (this.isConnected) {
      return
    }

    console.log('TIMESERIES CONNECT')
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

  public async execute<T>(
    selector: TimeseriesContext,
    query: string,
    params: unknown[]
  ): Promise<QueryResult<T>> {
    // TODO: real logic for selecting shard
    const tsName = `${selector.nodeType}$${selector.field}`
    const shards = this.tsCache.index[tsName]
    if (!shards[0]) {
      // TODO: implicitly create? or error and create it in the catch?
      throw new Error(`Timeseries ${tsName} does not exist`)
    }

    const pgInstance = shards[0].descriptor
    return this.pg.execute(pgInstance, query, params)
  }
}
