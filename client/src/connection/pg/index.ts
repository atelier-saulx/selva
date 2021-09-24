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
      // TODO: init client
    }

    return this.clients[selector]
  }
}

export default PGConnection
