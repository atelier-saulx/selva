import { ServerOptions } from '../../types'
import { connect, SelvaClient } from '@saulx/selva'
import { PG } from '../pg'

export type TimeSeriesInsertContext = {
  nodeId: string
  nodeType: string
  field: string
  fieldSchema: Record<string, unknown> & { type: string }
  payload: Record<string, unknown>
  ts: number
}

export const SELVA_TO_SQL_TYPE = {
  float: 'DOUBLE PRECISION',
  boolean: 'BOOLEAN',
  number: 'DOUBLE PRECISION',
  int: 'integer',
  string: 'text',
  text: 'jsonb', // because of localization
  json: 'jsonb',
  id: 'text',
  digest: 'text',
  url: 'text',
  email: 'text',
  phone: 'text',
  geo: 'jsonb',
  type: 'text',
  timestamp: 'TIMESTAMP',
  reference: 'test',
  references: 'JSONB',
  object: 'JSONB',
  record: 'JSONB',
  array: 'JSONB',
}

export class TimeseriesWorker {
  private opts: ServerOptions
  private client: SelvaClient
  private connectionString: string
  private postgresClient: PG

  constructor(opts: ServerOptions, connectionString: string) {
    this.opts = opts
    this.connectionString = connectionString
  }

  async start() {
    this.client = connect(this.opts.registry)
    this.postgresClient = new PG({
      connectionString: this.connectionString,
    })
    this.tick()
  }

  async tick() {
    console.log('timeseries tick')
    try {
      const row = await this.client.redis.rpop(
        { type: 'timeseriesQueue' },
        'timeseries_inserts'
      )
      if (row) {
        console.log(row)
        const obj = JSON.parse(row)
        const { type, context } = obj

        if (type === 'insert') {
          await this.insertToTimeSeriesDB(context)
        } else {
          console.error(`Unknown schema event ${type} for ${row}`)
        }
      }
    } catch (e) {
      console.error(e)
      //nop
    }
    setTimeout(this.tick.bind(this), 1000)
  }

  getTableName(context: TimeSeriesInsertContext): string {
    return `"${context.nodeType}\$${context.field}"`
  }

  async ensureTableExists(context: TimeSeriesInsertContext): Promise<void> {
    const createTable = `
    CREATE TABLE IF NOT EXISTS ${this.getTableName(context)} (
      "nodeId" text,
      payload ${SELVA_TO_SQL_TYPE[context.fieldSchema.type]},
      ts TIMESTAMP,
      "fieldSchema" jsonb
    );
    `
    console.log(`running: ${createTable}`)
    await this.postgresClient.execute<void>(createTable, [])

    const createNodeIdIndex = `CREATE INDEX IF NOT EXISTS "${this.getTableName(
      context
    ).slice(1, -1)}_node_id_idx" ON ${this.getTableName(context)} ("nodeId");`

    console.log(`running: ${createNodeIdIndex}`)
    await this.postgresClient.execute<void>(createNodeIdIndex, [])
  }

  async insertToTimeSeriesDB(context: TimeSeriesInsertContext) {
    console.log(`Inserting data, postgres at ${this.connectionString}`)

    await this.ensureTableExists(context)

    await this.postgresClient.execute(
      `INSERT INTO ${this.getTableName(
        context
      )} ("nodeId", payload, ts, "fieldSchema") VALUES ($1, $2, $3, $4)`,
      [
        context.nodeId,
        context.payload,
        new Date(context.ts),
        context.fieldSchema,
      ]
    )
  }

  async destroy() {
    await this.client.destroy()
  }
}

export async function startTimeseriesWorker(
  opts: ServerOptions,
  timeseriesDbInfo: { password: string; host: string; port: number }
): Promise<TimeseriesWorker> {
  const connectionString = `postgres://postgres:${timeseriesDbInfo.password}@${timeseriesDbInfo.host}:${timeseriesDbInfo.port}`
  const worker = new TimeseriesWorker(opts, connectionString)
  await worker.start()
  return worker
}
