import ProcessManager from './processManager'
import { spawnSync } from 'child_process'
import { Client as PgClient } from 'pg'
import * as path from 'path'

const IS_PROD: boolean = process.env.CLOUD && process.env.CLOUD !== 'local'

// TODO: increase this? and make the worker update it when it creates new tables
const TABLE_META_COLLECT_INTERVAL = 0.3 * 60 * 1e3

function sleep(ms: number) {
  return new Promise((resolve) => setTimeout(resolve, ms))
}

export default class PostgresManager extends ProcessManager {
  private pgPort: number
  private pgClient: PgClient
  private name: string
  private lastRun: number
  private tout: NodeJS.Timeout

  constructor({
    port,
    password,
    name,
  }: {
    port: number
    password: string
    name: string
  }) {
    const isLinux = process.platform === 'linux'

    if (isLinux) {
      const command = `/usr/lib/postgresql/12/bin/postgres`
      super(command, {
        // TODO: pg_data needs to more easily be pluggable
        args: ['-D', process.env.PGDATA, '-p', String(port), '-h', '0.0.0.0'],
        env: {},
      })
    } else {
      const command = `docker`
      const args = [
        `run`,
        `--rm`,
        `--name`,
        name,
        `-e`,
        `POSTGRES_PASSWORD=${password}`,
        `-p`,
        `${port}:5432`,
        `postgres:14beta2-alpine3.14`,
      ]
      super(command, {
        args,
        env: {},
      })
    }

    this.pgPort = port
    this.name = name
    this.lastRun = Date.now()
  }

  getPort(): number {
    return this.pgPort
  }

  destroy(signal?: NodeJS.Signals) {
    if (IS_PROD) {
      if (this.tout) {
        clearTimeout(this.tout)
        this.tout = undefined
      }

      return
    }

    spawnSync(`docker`, [`rm`, `-f`, this.name])
    super.destroy(signal)
  }

  protected async collect(): Promise<any> {
    const runtimeInfo = await super.collect()

    try {
      let timeout
      const wait = () =>
        new Promise((_resolve, reject) => {
          timeout = setTimeout(
            () => reject(new Error('Info-timeout took longer then 2 seconds')),
            5e3
          )
        })

      const now = Date.now()
      if (now - this.lastRun > TABLE_META_COLLECT_INTERVAL) {
        this.lastRun = now

        const info = await Promise.race([wait(), this.getTableMeta()])
        clearTimeout(timeout)
        console.log('DO SOMETHING WITH THIS', runtimeInfo, info)

        return { pgInfo: info, runtimeInfo }
      } else {
        return { runtimeInfo }
      }
    } catch (err: any) {
      // this.emit('error', err)
      return {
        runtimeInfo,
        err: err.message,
        isBusy: true,
      }
    }
  }

  async getTableMeta() {
    const tableQ = `
SELECT tablename
FROM pg_catalog.pg_tables
WHERE schemaname != 'pg_catalog' AND 
      schemaname != 'information_schema';`
    const sizeQ = `SELECT pg_table_size(quote_ident($1)), pg_total_relation_size(quote_ident($2));`

    const tables = await this.pgClient.query(tableQ, [])
    const rows = tables.rows.filter(({ tablename }) => tablename.includes('$'))
    const metaRows = await Promise.all(
      rows.map(async ({ tablename }) => {
        const r = await this.pgClient.query(sizeQ, [tablename, tablename])
        return {
          tablename,
          tableSizeBytes: Number(r.rows[0].pg_table_size),
          relationSizeBytes: Number(r.rows[0].pg_total_relation_size),
        }
      })
    )
    const metaObj = metaRows.reduce((acc, x) => {
      return { ...acc, ...{ [x.tablename]: x } }
    }, {})

    return metaObj
  }

  async createClient() {
    // TODO:
    const password = 'baratta'
    const host = '127.0.0.1'

    let ctr = 0
    while (ctr < 1000) {
      ++ctr
      try {
        this.pgClient = new PgClient({
          connectionString: `postgres://postgres:${password}@${host}:${this.pgPort}`,
        })
        await this.pgClient.connect()
        await this.pgClient.query(`select 1`, [])
        break
      } catch (e) {
        // nop
      }
      await sleep(1000)
    }

    // Get meta as soon as server is ready
    this.lastRun = 0
  }

  start() {
    if (IS_PROD) {
      const measurementLoop = () => {
        this.tout = setTimeout(() => {
          this.getTableMeta()
            .then((data) => {
              this.emit('stats', { pgInfo: data })
            })
            .catch((e) => {
              // console.error(
              //   `Error collecting load measurements from ${this.command}`,
              //   e
              // )
            })
            .finally(() => {
              measurementLoop()
            })
        }, 10e3)
      }

      return
    }

    super.start()
    this.createClient()
  }
}
