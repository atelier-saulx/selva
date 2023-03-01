import { BigQuery, TableMetadata } from '@google-cloud/bigquery'

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

    const [job] = await this.client.createQueryJob({ query })
    const [rows] = await job.getQueryResults()

    return rows
  }

  public async insert(tableName: string, data: any[]) {
    return this.client.dataset('selva_timeseries').table(tableName).insert(data)
  }

  public async createTable(tableName, schema: TableMetadata) {
    return this.client
      .dataset('selva_timeseries')
      .createTable(tableName, schema)
  }
}

export default BQConnection
