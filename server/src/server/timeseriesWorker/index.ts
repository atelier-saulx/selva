import { ServerOptions } from '../../types'
import { connect, SelvaClient, constants } from '@saulx/selva'
import { PG } from '../pg'

export type TimeSeriesInsertContext = {
  nodeId: string
  nodeType: string
  field: string
  fieldSchema: any
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

  constructor(opts: ServerOptions, connectionString: string) {
    this.opts = opts
    this.connectionString = connectionString
  }

  async start() {
    this.client = connect(this.opts.registry)
    await this.client.pg.connect()
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
    return `"${context.nodeType}\$${context.field}$0"`
  }

  async ensureTableExists(context: TimeSeriesInsertContext): Promise<void> {
    if (this.client.pg.hasTimeseries(context)) {
      return
    }

    const createTable = `
    CREATE TABLE IF NOT EXISTS ${this.getTableName(context)} (
      "nodeId" text,
      payload ${SELVA_TO_SQL_TYPE[context.fieldSchema.type]},
      ts TIMESTAMP,
      "fieldSchema" jsonb
    );
    `
    console.log(`running: ${createTable}`)
    const pg = this.client.pg.getMinInstance()
    await pg.execute<void>(createTable, [])

    const createNodeIdIndex = `CREATE INDEX IF NOT EXISTS "${this.getTableName(
      context
    ).slice(1, -1)}_node_id_idx" ON ${this.getTableName(context)} ("nodeId");`

    console.log(`running: ${createNodeIdIndex}`)
    await pg.execute<void>(createNodeIdIndex, [])

    const { meta: current } = this.client.pg.tsCache.instances[pg.id]
    const stats = {
      cpu: current.cpu,
      memory: current.memory,
      timestamp: Date.now(),
      tableMeta: {
        [this.getTableName(context).slice(1, -1)]: {
          tableName: this.getTableName(context),
          tableSizeBytes: 0,
          relationSizeBytes: 0,
        },
      },
    }

    this.client.redis.publish(
      { type: 'timeseriesRegistry' },
      constants.TS_REGISTRY_UPDATE,
      JSON.stringify({
        event: 'new_shard',
        ts: Date.now(),
        id: pg.id,
        data: { stats },
      })
    )

    this.client.pg.tsCache.updateIndexByInstance(pg.id, { stats })
  }

  async insertToTimeSeriesDB(context: TimeSeriesInsertContext) {
    console.log(`Inserting data, postgres at ${this.connectionString}`)

    await this.ensureTableExists(context)

    // TODO: actual selection
    await this.client.pg.pg.execute<void>(
      '127.0.0.1:5436',
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

// TODO:
// we want to create a new nodeType$field$0 table if the timeseries doesn't exist when inserting
// if it exists, check if it's getting full and create a new shard with Date.now()
// then send event so clients update without delay
