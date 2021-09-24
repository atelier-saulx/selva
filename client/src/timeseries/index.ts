import { SelvaClient } from '..'
import PGConnection from '../connection/pg'
import TimeseriesCache from './timeseriesCache'

export class TimeseriesClient {
  private client: SelvaClient
  private pg: PGConnection
  private tsCache: TimeseriesCache

  constructor(client: SelvaClient) {
    // TODO: credentials
    this.client = client
    this.pg = new PGConnection({ user: 'postgres', password: 'baratta' })
    this.tsCache = new TimeseriesCache(client)
  }
}
