import { ServerOptions } from '../../types'
import { connect, SelvaClient } from '@saulx/selva'

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

  constructor(opts: ServerOptions) {
    this.opts = opts
  }

  async start() {
    this.client = connect(this.opts.registry)
    await this.client.pg.connect()
    this.tick()
  }

  async tick() {
    try {
      const row = await this.client.redis.rpop(
        { type: 'timeseriesQueue' },
        'timeseries_inserts'
      )
      if (row) {
        const obj: {
          type: string
          context: TimeSeriesInsertContext
        } = JSON.parse(row)
        const { type, context } = obj

        if (type === 'insert') {
          await this.client.pg.insert(context, context)
        } else {
          console.error(`Unknown schema event ${type} for ${row}`)
        }
      }
    } catch (e: any) {
      console.error(e)
      //nop
    }
    setTimeout(this.tick.bind(this), 1000)
  }

  async destroy() {
    await this.client.destroy()
  }
}

export async function startTimeseriesWorker(
  opts: ServerOptions
): Promise<TimeseriesWorker> {
  const worker = new TimeseriesWorker(opts)
  await worker.start()
  return worker
}
