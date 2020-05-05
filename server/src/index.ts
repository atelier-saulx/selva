import { Options, ServerOptions } from './types'
import { SelvaServer, startServer } from './server'

const resolveOpts = async (opts: Options): Promise<ServerOptions> => {
  if (typeof opts === 'function') {
    return opts()
  } else {
    return opts
  }
}

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
  if (opts.port && typeof opts.port !== 'number') {
    return `port is not a number ${opts.port}`
  }
}

export async function startOrigin(opts: Options): Promise<SelvaServer> {
  const parsedOpts = await resolveOpts(opts)
  const err = validate(parsedOpts, ['registry'], ['replica'])
  if (err) {
    throw new Error(err)
  }
  return startServer('origin', parsedOpts)
}

export async function startRegistry(opts: Options): Promise<SelvaServer> {
  const parsedOpts = await resolveOpts(opts)
  const err = validate(parsedOpts, [], ['registry', 'replica', 'backup'])
  if (err) {
    throw new Error(err)
  }
  return startServer('registry', parsedOpts)
}

// 1 extra new thing - monitor server / stats
export async function startReplica(opts: Options) {}

export async function startSubscriptionManager(opts: Options) {}

export async function start(opts: Options) {}
