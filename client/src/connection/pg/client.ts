import { native, QueryResult, Pool } from 'pg'
const PgPool = native.Pool // or use pool if native deps are an issue

export class PG {
  protected pool: Pool
  protected ctr: number

  public constructor({ connectionString }: { connectionString: string }) {
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
