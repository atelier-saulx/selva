import { isMainThread } from 'worker_threads'
import { native, QueryResult, Pool } from 'pg'

// TODO: why is this broken in worker_threads
const PgPool = isMainThread ? native.Pool : Pool

export class PG {
  public id: string
  protected pool: Pool
  protected ctr: number

  public constructor({
    id,
    connectionString,
  }: {
    id: string
    connectionString: string
  }) {
    this.id = id
    this.pool = new PgPool({ connectionString: connectionString })

    this.pool.on('error', (err: Error) => {
      console.error('PGPoolUnexpected error on idle client', err)
    })
  }

  public async execute<T>(
    query: string,
    params: unknown[]
  ): Promise<QueryResult<T>> {
    const client = await this.pool.connect()

    try {
      return await client.query(query, params || [])
    } finally {
      client.release()
    }
  }

  async close(): Promise<void> {
    if (this.pool) {
      await this.pool.end()
    }
  }
}
