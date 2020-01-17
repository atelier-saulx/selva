import { default as RedisClient, ConnectOptions } from './redis'
import { id, IdOptions } from './id'
import { set, SetOptions } from './set'
import { ModifyOptions, ModifyResult } from './modifyTypes'
import { deleteItem, DeleteOptions } from './delete'
import { get, GetOptions } from './get'
import { readFileSync } from 'fs'
import { join as pathJoin } from 'path'

// FIXME: this is pretty shit
let MODIFY_SCRIPT
try {
  MODIFY_SCRIPT = readFileSync(
    pathJoin(process.cwd(), 'dist', 'lua', 'modify.lua')
  )
} catch (e) {
  console.error(`Failed to read modify.lua ${e.stack}`)
  process.exit(1)
}

export class SelvaClient {
  public redis: RedisClient

  constructor(opts: ConnectOptions | (() => Promise<ConnectOptions>)) {
    this.redis = new RedisClient(opts)
  }

  async destroy() {
    this.redis.destroy()
  }

  // need client.destroy (at least for tests)

  id(props: IdOptions) {
    return id(this, props)
  }

  set(props: SetOptions) {
    return set(this, props)
  }

  get(props: GetOptions) {
    return get(this, props)
  }

  async modify(opts: ModifyOptions): Promise<ModifyResult> {
    return this.redis.loadAndEvalScript(
      'modify',
      MODIFY_SCRIPT,
      0,
      [],
      [JSON.stringify(opts)],
      { batchingEnabled: true }
    )
  }

  delete(props: DeleteOptions) {
    let hierarchy = true
    let id: string
    if (typeof props == 'object') {
      id = props.$id
      if (props.$hierarchy === false) {
        hierarchy = false
      }
    } else {
      id = props
    }
    return deleteItem(this, id, hierarchy)
  }
}

export function connect(
  opts: ConnectOptions | (() => Promise<ConnectOptions>)
): SelvaClient {
  return new SelvaClient(opts)
}
