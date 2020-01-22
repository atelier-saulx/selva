import { default as RedisClient, ConnectOptions } from './redis'
// import { id, IdOptions } from './id'
import { set, SetOptions } from './set'
import { ModifyOptions, ModifyResult } from './modifyTypes'
import { deleteItem, DeleteOptions } from './delete'
import { get, GetOptions } from './get'
import { readFileSync } from 'fs'
import { join as pathJoin } from 'path'
import { Schema, Id } from './schema'
import { updateSchema } from './schema/updateSchema'
import { getSchema } from './schema/getSchema'
import getTypeFromId from './getTypeFromId'
import digest from './digest'
import { IdOptions } from '../lua/src/id'

// FIXME: this is pretty shit
let MODIFY_SCRIPT
let ID_SCRIPT
try {
  MODIFY_SCRIPT = readFileSync(
    pathJoin(process.cwd(), 'dist', 'lua', 'modify.lua')
  )
  ID_SCRIPT = readFileSync(pathJoin(process.cwd(), 'dist', 'lua', 'id.lua'))
} catch (e) {
  console.error(`Failed to read modify.lua ${e.stack}`)
  process.exit(1)
}

export class SelvaClient {
  public redis: RedisClient

  constructor(opts: ConnectOptions | (() => Promise<ConnectOptions>)) {
    this.redis = new RedisClient(opts)
  }

  digest(payload: string) {
    return digest(payload)
  }

  async destroy() {
    this.redis.destroy()
  }

  async id(props: IdOptions): Promise<string> {
    // move to js
    return this.redis.loadAndEvalScript(
      'modify',
      ID_SCRIPT,
      0,
      [],
      [JSON.stringify(props)],
      {
        batchingEnabled: true
      }
    )
  }

  async set(props: SetOptions) {
    return set(this, props)
  }

  async get(props: GetOptions) {
    return get(this, props)
  }

  async updateSchema(props: Schema) {
    return updateSchema(this, props)
  }

  async getSchema() {
    return getSchema(this)
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

  async getTypeFromId(id: Id) {
    return getTypeFromId(this, id)
  }

  async delete(props: DeleteOptions) {
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
