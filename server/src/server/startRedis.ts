import { SelvaServer } from './'
import { ServerOptions } from '../types'
import path from 'path'
import fs from 'fs'
import { spawn, execSync } from 'child_process'
import chalk from 'chalk'
import RedisManager from './redisManager'

// this is only for the 'raw' redis
// no handling of registry, no different types, no subscriptions stuff
// has to be replaced with a nice wrapper that makes it a little bit more reliable
export default (server: SelvaServer, opts: ServerOptions) => {
  const { port, dir, modules, host } = opts

  if (opts.attachToExisting) {
    return
  }

  const args = ['--port', String(port), '--protected-mode', 'no', '--dir', dir]

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
    console.log(server.origin)
    args.push('--replicaof', server.origin.host, String(server.origin.port))
  }

  const tmpPath = path.join(process.cwd(), './tmp')
  if (!fs.existsSync(tmpPath)) {
    fs.mkdirSync(tmpPath)
  }

  try {
    execSync(`redis-cli -p ${port} config set dir ${dir}`, { stdio: 'ignore' })
    execSync(`redis-cli -p ${port} shutdown`)
  } catch (_err) {}

  server.pm = new RedisManager(args, {
    port,
    host,
    name,
    type: server.type,
    selvaClient: server.selvaClient
  })
  server.pm.start()
  server.pm.on('stdout', s => server.emit('stdout', s))
  server.pm.on('stderr', s => server.emit('stderr', s))

  server.pm.on('stats', o => server.emit('stats', o))
  // const redisDb = spawn('redis-server', args)

  // // not so nice
  // const emit = (...args) => {
  //   server.emit('data', ...args)
  // }
  // redisDb.stderr.on('data', emit)
  // redisDb.stdout.on('data', emit)

  // redisDb.stdout.on('close', () => {
  //   console.log(chalk.blue(`Redis server on ${port} closed`))
  //   server.emit('close', ...args)
  // })

  // want to make nice nice
}
