import { ConnectOptions } from '@saulx/selva'
import { SelvaServer } from './'
import { ServerOptions } from '../types'

export default async (server: SelvaServer, opts: ServerOptions) => {
  console.info(`Start SelvaServer ${server.type} on port ${opts.port} 🌈`)
  const { port, dir, modules } = opts

  const args = ['--port', String(port), '--protected-mode', 'no', '--dir', dir]
}
