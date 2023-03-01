import { BigQuery, TableMetadata } from '@google-cloud/bigquery'
import { TimeseriesContext } from '../../timeseries'

export function getTableName(tsCtx: TimeseriesContext): string {
  return tsCtx.nodeType + '___' + tsCtx.field
}

class BQConnection {
  private client: BigQuery

  constructor() {
    if (!process.env.GOOGLE_APPLICATION_CREDENTIALS) {
      console.warn(
        'GOOGLE_APPLICATION_CREDENTIALS must be set as an env variable'
      )

      return
    }

    this.client = new BigQuery()
  }

  public async execute(query: string, params: unknown[]): Promise<any[]> {
    console.log('SQL', query, params)

    const [job] = await this.client.createQueryJob({
      query,
      location: 'europe-west3',
      params,
    })
    const [rows] = await job.getQueryResults()

    return rows
  }

  public async insert(tsCtx: TimeseriesContext, data: any[]) {
    const transformed = data.map((d) => {
      const { nodeId, payload, ts } = d
      return {
        nodeId,
        ts: BigQuery.datetime(new Date(ts).toISOString()),
        payload:
          typeof payload === 'object' ? JSON.stringify(payload) : payload,
        fieldSchema: JSON.stringify(tsCtx.fieldSchema || {}),
      }
    })

    console.log('INSERTINg', JSON.stringify(transformed, null, 2))

    return this.client
      .dataset('selva_timeseries', { location: 'europe-west3' })
      .table(getTableName(tsCtx))
      .insert(transformed)
  }

  public async createTable(tsCtx: TimeseriesContext, schema: TableMetadata) {
    return this.client
      .dataset('selva_timeseries', { location: 'europe-west3' })
      .createTable(getTableName(tsCtx), schema)
  }
}

export default BQConnection
