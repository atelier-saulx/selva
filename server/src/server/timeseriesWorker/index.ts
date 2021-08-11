import { ServerOptions } from '../../types'
import { connect, SelvaClient } from '@saulx/selva'

export class TimeseriesWorker {
  private opts: ServerOptions
  private client: SelvaClient

  constructor(opts: ServerOptions) {
    this.opts = opts
  }

  async start() {
    this.client = connect(this.opts.registry)
    this.tick()
  }

  async tick() {
    console.log('timeseries tick')
    try {
      const row = await this.client.redis.rpop({ name: 'timeseries' }, 'timeseries_inserts')
      if (row) {
        console.log(row)
        const obj = JSON.parse(row)
        // TODO: insert to PG
      }
    } catch (e) {
      console.error(e)
      //nop
    }
    setTimeout(this.tick.bind(this), 1000)
  }

  async destroy() {
    await this.client.destroy()
  }
}
export async function startTimeseriesWorker(opts: ServerOptions): Promise<TimeseriesWorker> {
  console.log('aaaaaa')
  const worker = new TimeseriesWorker(opts)
  await worker.start()
  return worker
}
