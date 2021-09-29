import { Options, ServerOptions } from './types'
import { SelvaServer, startServer } from './server'
import getPort from 'get-port'
import chalk from 'chalk'
import os from 'os'
import { join } from 'path'
import fs from 'fs'
import { TextServer } from './server/text'
import mkdirp from 'mkdirp'

export * as s3Backups from './backup-plugins/s3'

const resolveOpts = async (opts: Options): Promise<ServerOptions> => {
  let parsedOpts: ServerOptions
  if (typeof opts === 'function') {
    parsedOpts = await opts()
  } else {
    parsedOpts = await opts
  }
  if (!parsedOpts.port) {
    parsedOpts.port = await getPort()
  }

  if (!parsedOpts.host) {
    const network = os.networkInterfaces()
    let ip
    for (let key in network) {
      const r = network[key].find(
        (v) => v.family === 'IPv4' && v.internal === false
      )
      if (r) {
        ip = r
        break
      }
    }
    parsedOpts.host = (ip && ip.address) || '0.0.0.0'
  }

  if (!parsedOpts.dir) {
    parsedOpts.dir = join(process.cwd(), 'tmp')
  }

  // has to be mkdirp
  if (!fs.existsSync(parsedOpts.dir)) {
    await mkdirp(parsedOpts.dir)
  }

  if (parsedOpts.modules) {
    if (Array.isArray(parsedOpts.modules)) {
      parsedOpts.modules = [
        ...new Set([...defaultModules, ...parsedOpts.modules]),
      ]
    }
  } else {
    parsedOpts.modules = defaultModules
  }

  if (parsedOpts.default) {
    parsedOpts.name = 'default'
  }

  return parsedOpts
}

const defaultModules = ['selva']

const validate = (
  opts: ServerOptions,
  required: string[],
  illegal: string[]
): string | undefined => {
  for (const field of required) {
    if (!opts[field]) {
      return `${field} is required`
    }
  }

  for (const field of illegal) {
    if (opts[field]) {
      return `${field} is not a valid option`
    }
  }

  if (opts.name === 'registry') {
    return `Registry is a reserved name`
  }

  if (!opts.port) {
    return `no port provided`
  }

  if (!opts.host) {
    return `no host provided`
  }

  if (typeof opts.port !== 'number') {
    return `port is not a number ${opts.port}`
  }

  if (typeof opts.dir !== 'string') {
    return `string is not a string ${opts.dir}`
  }

  if (!Array.isArray(opts.modules)) {
    return `Modules needs to be an array of strings`
  }
}

export async function startOrigin(opts: Options): Promise<SelvaServer> {
  const parsedOpts = await resolveOpts(opts)
  const err = validate(parsedOpts, ['registry', 'name'], [])
  if (err) {
    console.error(`Error starting origin selva server ${chalk.red(err)}`)
    throw new Error(err)
  }
  if (!parsedOpts.name) {
    parsedOpts.name = 'default'
  }
  return startServer('origin', parsedOpts)
}

export async function startRegistry(opts: Options): Promise<SelvaServer> {
  const parsedOpts = await resolveOpts(opts)

  const err = validate(
    parsedOpts,
    [],
    ['registry', 'backups', 'name', 'default']
  )

  parsedOpts.name = 'registry'

  if (err) {
    console.error(`Error starting registry selva server ${chalk.red(err)}`)
    throw new Error(err)
  }
  return startServer('registry', parsedOpts)
}

// 1 extra new thing - monitor server / stats
export async function startReplica(opts: Options) {
  const parsedOpts = await resolveOpts(opts)

  const err = validate(parsedOpts, ['registry', 'name'], ['backups'])
  if (err) {
    console.error(`Error starting replica selva server ${chalk.red(err)}`)
    throw new Error(err)
  }
  if (!parsedOpts.name && parsedOpts.default) {
    parsedOpts.name = 'default'
  }
  return startServer('replica', parsedOpts)
}

export async function startSubscriptionManager(opts: Options) {
  const parsedOpts = await resolveOpts(opts)
  const err = validate(parsedOpts, ['registry'], ['name', 'default', 'backups'])

  parsedOpts.name = 'subscriptionManager'

  if (err) {
    console.error(
      `Error starting subscription Mmnager selva server ${chalk.red(err)}`
    )
    throw new Error(err)
  }
  return startServer('subscriptionManager', parsedOpts)
}

export async function startTextServer(opts: Options) {
  const parsedOpts = await resolveOpts(opts)

  const server = new TextServer()
  server.start(parsedOpts)
  return server
}

export async function startSubscriptionRegistry(opts: Options) {
  const parsedOpts = await resolveOpts(opts)
  const err = validate(parsedOpts, ['registry'], ['name', 'default', 'backups'])
  parsedOpts.name = 'subscriptionRegistry'
  if (err) {
    console.error(
      `Error starting subscription Registry selva server ${chalk.red(err)}`
    )
    throw new Error(err)
  }
  return startServer('subscriptionRegistry', parsedOpts)
}

// make a registry, then add origin, then add subs manager
// backups may be a bit problematic here :/
// maybe we can put the registry and subs manager in a different db in redis and only back up the "main db"? hmmmmmmmmmmmmm let me see (tony notes)
export async function start(opts: Options) {
  const parsedOpts = await resolveOpts(opts)

  // TODO: for now all in different ports, fix later
  const err = validate(
    parsedOpts,
    [],
    ['registry', 'backups', 'name', 'default']
  )

  if (err) {
    console.error(`Error starting selva server ${chalk.red(err)}`)
    throw new Error(err)
  }

  const registry = await startServer('registry', {
    ...parsedOpts,
    name: 'registry',
  })

  const origin = await startOrigin({
    name: 'default',
    registry,
    // @ts-ignore
    dir: opts.dir,
    selvaOptions: parsedOpts.selvaOptions,
    pipeRedisLogs: parsedOpts.pipeRedisLogs || {
      stdout: true,
      stderr: true,
    },
  })

  const subs = await startSubscriptionManager({
    registry: {
      port: parsedOpts.port,
      host: parsedOpts.host,
    },
  })

  const subsRegistry = await startSubscriptionRegistry({
    registry: {
      port: parsedOpts.port,
      host: parsedOpts.host,
    },
  })

  registry.on('close', async () => {
    // TODO: Remove comment
    // console.log('Close all servers does it work ?')
    await origin.destroy()
    await subs.destroy()
    await subsRegistry.destroy()
  })

  return registry
}

export { SelvaServer }
