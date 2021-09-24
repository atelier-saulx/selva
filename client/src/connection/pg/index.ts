import { PG } from './client'

class PGConnection {
  private clients: {
    [id: string]: PG
  }

  constructor() {
    this.clients = {}
  }
}

export default PGConnection
