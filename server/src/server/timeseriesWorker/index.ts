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
      const rows = await this.client.redis.rpop(
        { type: 'timeseriesQueue' },
        'timeseries_inserts',
        100
      )

      const ops: Record<string, any[]> = {}
      rows.forEach((row) => {
        if (row) {
          const obj: {
            type: string
            context: TimeSeriesInsertContext
          } = JSON.parse(row)
          const { type, context } = obj

          if (type === 'insert') {
            const k = context.nodeType + '|' + context.field
            if (!ops[k]) {
              ops[k] = []
            }

            ops[k].push(context)
          } else {
            console.error(`Unknown schema event ${type} for ${row}`)
          }
        }
      })

      await Promise.all(
        Object.entries(ops).map((_table, rows) => {
          return this.client.pg.insert(rows[0], rows)
        })
      )
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
