import { SelvaClient } from '..'
import PGConnection from '../connection/pg'
import TimeseriesCache from './timeseriesCache'

type TimeseriesContext = {
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
    await this.tsCache.subscribe()
    this.isConnected = true
  }

  disconnect() {
    this.tsCache.unsubscribe()
    this.isConnected = false
  }
}
