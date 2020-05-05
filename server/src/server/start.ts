import { ConnectOptions } from '@saulx/selva'
import { SelvaServer } from './'
import { ServerOptions } from '../types'
import path from 'path'
import fs from 'fs'
import { spawn, execSync } from 'child_process'

export default async (server: SelvaServer, opts: ServerOptions) => {
  console.info(`Start SelvaServer ${server.type} on port ${opts.port} 🌈`)
  const { port, dir, modules } = opts

  const args = ['--port', String(port), '--protected-mode', 'no', '--dir', dir]

  modules.forEach(m => {
    const platform = process.platform + '_' + process.arch
    const p =
      // if it contains ".so"" then dont do this
      m.indexOf('.so') !== -1
        ? m
        : path.join(
            __dirname,
            '../',
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

  // if (replica // connect) {

  // is connectoptions
  //   args.push('--replicaof', replica.host, String(replica.port))
  // }

  const tmpPath = path.join(process.cwd(), './tmp')
  if (!fs.existsSync(tmpPath)) {
    fs.mkdirSync(tmpPath)
  }

  try {
    execSync(`redis-cli -p ${port} config set dir ${dir}`)
    execSync(`redis-cli -p ${port} shutdown`)
  } catch (e) {}

  server.port = opts.port

  const redisDb = spawn('redis-server', args)

  const emit = (...args) => {
    server.emit('data', ...args)
  }

  redisDb.stderr.on('data', emit)
  redisDb.stdout.on('data', emit)
  redisDb.on('close', (...args) => {
    console.log('redis closed?')
    server.emit('close', ...args)

  // want to make nice nice
  console.log(redisDb)
}
