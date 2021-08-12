import { ServerOptions } from '../../types'
import { connect, SelvaClient } from '@saulx/selva'
import { PG } from '../pg'

export type TimeSeriesInsertContext = {
  nodeId: string
  nodeType: string
  field: string
  fieldSchema: Record<string, unknown>
  payload: Record<string, unknown>
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
    await this.postgresClient.execute<void>(
      `
CREATE TABLE IF NOT EXISTS demo_events (
  "nodeId" text,
  "nodeType" text,
  field text,
  "fieldSchema" jsonb,
  "payload" jsonb
)`,
      []
    )
    this.tick()
  }

  async tick() {
    console.log('timeseries tick')
    try {
      const row = await this.client.redis.rpop({ name: 'timeseries' }, 'timeseries_inserts')
      if (row) {
        console.log(row)
        const obj = JSON.parse(row)
        const { type, context } = obj

        if (type === 'insert') {
          await this.insertToTimeseriesDB(context)
        } else {
          console.error(`Unknown schema event ${type} for ${row}`)
        }
        // TODO: insert to PG
      }
    } catch (e) {
      console.error(e)
      //nop
    }
    setTimeout(this.tick.bind(this), 1000)
  }

  // {"type":"insert","context":{"nodeId":"viA","nodeType":"lekkerType","field":"auth","fieldSchema":{"timeseries":true,"type":"json"},"payload":{"role":{"id":["root"],"type":"admin"}},"ts":1628756805739}}
  async insertToTimeseriesDB(context: TimeSeriesInsertContext) {
    console.log(`Inserting data, postgres at ${this.connectionString}`)
    await this.postgresClient.execute(
      `INSERT INTO demo_events ("nodeId", "nodeType", field, "fieldSchema", payload) VALUES ($1, $2, $3, $4, $5)`,
      [context.nodeId, context.nodeType, context.field, context.fieldSchema, context.payload]
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
  console.log('aaaaaa')
  const connectionString = `postgres://postgres:${timeseriesDbInfo.password}@${timeseriesDbInfo.host}:${timeseriesDbInfo.port}`
  const worker = new TimeseriesWorker(opts, connectionString)
  await worker.start()
  return worker
}
