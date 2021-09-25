import { ServerOptions } from '../../types'
import { connect, SelvaClient, constants } from '@saulx/selva'

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
    // TODO: use tsCache to figure out last shard
    return `"${context.nodeType}\$${context.field}$0"`
  }

  async ensureTableExists(context: TimeSeriesInsertContext): Promise<void> {
    if (this.client.pg.hasTimeseries(context)) {
      console.log('ALREADY EXISTS', context)
      return
    }

    const tsName = `${context.nodeType}$${context.field}`
    const tableName = `${tsName}$0`

    const createTable = `
    CREATE TABLE IF NOT EXISTS "${tableName}" (
      "nodeId" text,
      payload ${SELVA_TO_SQL_TYPE[context.fieldSchema.type]},
      ts TIMESTAMP,
      "fieldSchema" jsonb
    );
    `
    console.log(`running: ${createTable}`)
    const pg = this.client.pg.getMinInstance()
    await pg.execute<void>(createTable, [])

    const createNodeIdIndex = `CREATE INDEX IF NOT EXISTS "${tableName}_node_id_idx" ON "${tableName}" ("nodeId");`

    console.log(`running: ${createNodeIdIndex}`)
    await pg.execute<void>(createNodeIdIndex, [])

    const { meta: current } = this.client.pg.tsCache.instances[pg.id]
    const stats = {
      cpu: current.cpu,
      memory: current.memory,
      timestamp: Date.now(),
      tableMeta: {
        [tableName]: {
          tableName,
          tableSizeBytes: 0,
          relationSizeBytes: 0,
        },
      },
    }

    this.client.pg.tsCache.updateIndexByInstance(pg.id, { stats })
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
  }

  async insertToTimeSeriesDB(context: TimeSeriesInsertContext) {
    console.log(`Inserting data, postgres at ${this.connectionString}`)

    await this.ensureTableExists(context)

    await this.client.pg.insert<void>(
      context,
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
// create new shard if current is full
