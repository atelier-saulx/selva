import { SelvaServer } from './'
import { ServerOptions } from '../types'
import path from 'path'
import fs from 'fs'
import { execSync } from 'child_process'
import RedisManager from './redisManager'
import killBlockingScripts from './killBlockingScripts'

// this is only for the 'raw' redis
// no handling of registry, no different types, no subscriptions stuff
// has to be replaced with a nice wrapper that makes it a little bit more reliable
export default (server: SelvaServer, opts: ServerOptions) => {
  const { port, dir, modules, host } = opts

  if (opts.attachToExisting) {
    return
  }

  const args = ['--port', String(port), '--protected-mode', 'no', '--dir', dir]

  if (opts.save) {
    if (opts.save === true) {
      args.push('--save', '900', '1')
      args.push('--save', '300', '10')
    } else {
      args.push('--save', String(opts.save.seconds), String(opts.save.changes))
    }
  }

  modules.forEach(m => {
    const platform = process.platform + '_' + process.arch
    const p =
      // if it contains ".so"" then don't do this
      m.indexOf('.so') !== -1
        ? m
        : path.join(
            __dirname,
            '../../',
            'modules',
            'binaries',
            platform,
            m + '.so'
          )
    if (fs.existsSync(p)) {
      args.push('--loadmodule', p)
    } else {
      console.warn(`${m} module does not exists for "${platform}"`)
    }
  })

  if (server.type === 'replica') {
    args.push('--replicaof', server.origin.host, String(server.origin.port))
  }

  try {
    execSync(`redis-cli -p ${port} config set dir ${dir}`, { stdio: 'ignore' })
    execSync(`redis-cli -p ${port} shutdown`, { stdio: 'ignore' })
  } catch (_err) {}

  server.pm = new RedisManager(args, {
    port,
    host,
    name: server.name,
    type: server.type,
    selvaClient: server.selvaClient
  })

  killBlockingScripts(host, port)

  server.pm.start()
  server.pm.on('error', err => server.emit('error', err))
  server.pm.on('stdout', s => server.emit('stdout', s))
  server.pm.on('stderr', s => server.emit('stderr', s))
  server.pm.on('stats', o => server.emit('stats', o))
}
