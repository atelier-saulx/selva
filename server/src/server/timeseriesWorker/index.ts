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

  async insertToTimeSeriesDB(context: TimeSeriesInsertContext) {
    console.log(`Inserting data, postgres at ${this.connectionString}`)

    // TODO: no need to send boring query like this
    await this.client.pg.insert<void>(
      context,
      `INSERT INTO $table_name ("nodeId", payload, ts, "fieldSchema") VALUES ($1, $2, $3, $4)`,
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
