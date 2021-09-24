import { PG } from './client'

type PGGlobalOpts = { user: string; password: string }
type PGSelector = string | { host: string; port: number }

class PGConnection {
  private opts: PGGlobalOpts
  private clients: {
    [id: string]: PG
  }

  constructor(opts: PGGlobalOpts) {
    this.opts = opts
    this.clients = {}
  }

  getClient(selector: PGSelector) {
    if (typeof selector === 'object') {
      const { host, port } = selector
      selector = `${host}:${port}`
    }

    if (!this.clients[selector]) {
      const connectionString = `postgres://${this.opts.user}:${this.opts.password}@${selector}`
      this.clients[selector] = new PG({
        connectionString,
      })
    }

    return this.clients[selector]
  }
}

export default PGConnection
