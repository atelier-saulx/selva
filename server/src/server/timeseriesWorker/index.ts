import { ServerOptions } from '../../types'
import { connect, SelvaClient } from '@saulx/selva'
import { v4 as uuid } from 'uuid'

const CONSUMER_ID = uuid()
const JOB_TIMEOUT = 1 * 60e3

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

    await this.client.redis.xgroup(
      { type: 'timeseriesQueue' },
      'CREATE',
      'timeseries_inserts',
      'executors',
      '0',
      'MKSTREAM'
    )

    this.tick()
  }

  async checkNew(): Promise<any[]> {
    const res = await this.client.redis.xreadgroup(
      { type: 'timeseriesQueue' },
      'GROUP',
      'executors',
      CONSUMER_ID,
      'COUNT',
      '100',
      'BLOCK',
      '1000',
      'STREAMS',
      'timeseries_inserts',
      '>'
    )

    if (!res || !res.length) {
      return []
    }

    const stream = res[0]
    if (!stream.length) {
      return []
    }

    const entries = stream[1]

    if (!entries.length) {
      return []
    }

    return entries.map((entry) => {
      const id = entry[0]
      const fields = entry[1]
      const obj = fields[3]

      return { id, obj }
    })
  }

  async checkPending(): Promise<any[]> {
    const allPending = await this.client.redis.xpending(
      { type: 'timeseriesQueue' },
      'timeseries_inserts',
      'executors',
      // 'IDLE',
      // String(JOB_TIMEOUT),
      '-',
      '+',
      '100'
    )

    if (
      !allPending ||
      allPending.length < 2 ||
      !allPending[0] ||
      !allPending[0].length
    ) {
      return []
    }

    const pendingIds = allPending[0].map((e) => e?.[0])
    const res = await this.client.redis.xclaim(
      { type: 'timeseriesQueue' },
      'timeseries_inserts',
      'executors',
      CONSUMER_ID,
      JOB_TIMEOUT,
      ...pendingIds
    )

    // console.log('RES', res)

    if (!res || !res.length) {
      return []
    }

    const stream = res[0]
    if (!stream.length) {
      return []
    }

    const resp = stream[1]

    if (!resp.length) {
      return []
    }

    const entries = resp[0]
    return entries.map((entry) => {
      const id = entry[1]
      const obj = entry[3]

      return { id, obj }
    })
  }

  async ack(id) {
    await this.client.redis.xack(
      { type: 'timeseriesQueue' },
      'timeseries_inserts',
      'executors',
      id
    )
  }

  async tick() {
    try {
      // const rows = await this.client.redis.rpop(
      //   { type: 'timeseriesQueue' },
      //   'timeseries_inserts',
      //   100
      // )
      let rows = await this.checkNew()

      if (!rows?.length) {
        rows = await this.checkPending()
      }

      const ops: Record<string, { rows: any[]; ids: string[] }> = {}
      rows.forEach((row, i) => {
        if (row) {
          const obj: {
            type: string
            context: TimeSeriesInsertContext
          } = JSON.parse(row.obj)
          const { type, context } = obj

          if (type === 'insert') {
            const k = context.nodeType + '|' + context.field
            if (!ops[k]) {
              ops[k] = { rows: [], ids: [] }
            }

            ops[k].rows.push(context)
            ops[k].ids.push(row.id)
          } else {
            console.error(`Unknown schema event ${type} for ${row}`)
          }
        }
      })

      const resps = await Promise.all(
        Object.entries(ops).map(async ([_table, { rows, ids }]) => {
          const insertResp = await this.client.pg.insert(rows[0], rows)
          return { insertResp, ids }
        })
      )

      await Promise.all(
        resps.map(({ insertResp, ids }) => {
          const acks = insertResp
            .filter((resp) => {
              return resp.success && resp.idx >= 0
            })
            .map((resp) => {
              const id = ids[resp.idx]
              return this.ack(id)
            })

          return Promise.all(acks)
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
