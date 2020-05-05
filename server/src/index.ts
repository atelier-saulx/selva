import { Options, ServerOptions } from './types'
import { SelvaServer, startServer } from './server'
import getPort from 'get-port'
import chalk from 'chalk'

const resolveOpts = async (opts: Options): Promise<ServerOptions> => {
  let parsedOpts: ServerOptions
  if (typeof opts === 'function') {
    parsedOpts = await opts()
  } else {
    parsedOpts = await opts
  }
  if (!parsedOpts.port) {
    parsedOpts.port = await getPort()
    console.log('Generated port', parsedOpts.port)
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
    ['registry', 'replica', 'backups', 'name', 'main']
  )
  if (err) {
    console.error(`Error starting registry selva server ${chalk.red(err)}`)
    throw new Error(err)
  }
  return startServer('registry', parsedOpts)
}

// 1 extra new thing - monitor server / stats
export async function startReplica(opts: Options) {}

export async function startSubscriptionManager(opts: Options) {}

// make a registry, then add origin, then add subs manager
// backups may be a bit problematic here :/
export async function start(opts: Options) {}
