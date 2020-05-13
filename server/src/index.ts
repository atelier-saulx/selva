import { Options, ServerOptions } from './types'
import { SelvaServer, startServer } from './server'
import getPort from 'get-port'
import chalk from 'chalk'
import os from 'os'

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
        v => v.family === 'IPv4' && v.internal === false
      )
      if (r) {
        ip = r
        break
      }
    }
    parsedOpts.host = (ip && ip.address) || '0.0.0.0'
  }

  if (!parsedOpts.dir) {
    parsedOpts.dir = process.cwd()
  }

  if (parsedOpts.modules) {
    if (Array.isArray(parsedOpts.modules)) {
      parsedOpts.modules = [
        ...new Set([...defaultModules, ...parsedOpts.modules])
      ]
    }
  } else {
    parsedOpts.modules = defaultModules
  }

  if (parsedOpts.default && !parsedOpts.name) {
    parsedOpts.name = 'default'
  }

  return parsedOpts
}

const defaultModules = ['redisearch', 'selva']

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

  // default name is 'main'
  const err = validate(parsedOpts, ['registry', 'name'], ['replica'])
  if (err) {
    console.error(`Error starting origin selva server ${chalk.red(err)}`)
    throw new Error(err)
  }
  return startServer('origin', parsedOpts)
}

export async function startRegistry(opts: Options): Promise<SelvaServer> {
  const parsedOpts = await resolveOpts(opts)

  const err = validate(
    parsedOpts,
    [],
    ['registry', 'replica', 'backups', 'name', 'default']
  )

  parsedOpts.name = 'registry'

  if (err) {
    console.error(`Error starting registry selva server ${chalk.red(err)}`)
    throw new Error(err)
  }
  return startServer('registry', parsedOpts)
}

// 1 extra new thing - monitor server / stats
export async function startReplica(opts: Options) {}

export async function startSubscriptionManager(opts: Options) {
  const parsedOpts = await resolveOpts(opts)
  // default name is 'main'
  const err = validate(
    parsedOpts,
    ['registry'],
    ['replica', 'name', 'default', 'backups']
  )

  parsedOpts.name = 'subscriptionManager'

  if (err) {
    console.error(
      `Error starting subscription Mmnager selva server ${chalk.red(err)}`
    )
    throw new Error(err)
  }
  return startServer('subscriptionManager', parsedOpts)
}

// make a registry, then add origin, then add subs manager
// backups may be a bit problematic here :/
// maybe we can put the registry and subs manager in a different db in redis and only back up the "main db"? hmmmmmmmmmmmmm let me see (tony notes)
export async function start(opts: Options) {}
