import { PG } from './client'
import { QueryResult } from 'pg'

type PGGlobalOpts = { user: string; password: string }
type PGSelector = string | { host: string; port: number }

function selectorToId(selector: PGSelector): string {
  if (typeof selector === 'object') {
    const { host, port } = selector
    return `${host}:${port}`
  }

  return selector
}

// TODO: add cleanup based on this.lastUsed

class PGConnection {
  private opts: PGGlobalOpts
  private lastUsed: {
    [id: string]: number
  }

  private clients: {
    [id: string]: PG
  }

  constructor(opts: PGGlobalOpts) {
    this.opts = opts
    this.clients = {}
    this.lastUsed = {}
  }

  public getClient(selector: PGSelector) {
    const id = selectorToId(selector)

    if (!this.clients[id]) {
      const connectionString = `postgres://${this.opts.user}:${this.opts.password}@${id}`
      this.clients[id] = new PG({
        id,
        connectionString,
      })
    }

    this.lastUsed[id] = Date.now()
    return this.clients[id]
  }

  public async execute<T>(
    selector: PGSelector,
    query: string,
    params: unknown[]
  ): Promise<QueryResult<T>> {
    // console.log('SQL', query, params)
    const client = this.getClient(selector)
    return client.execute(query, params)
  }
}

export default PGConnection
