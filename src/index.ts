import { default as RedisClient, ConnectOptions } from './redis'
// import { id, IdOptions } from './id'
import { set, SetOptions } from './set'
import { ModifyOptions, ModifyResult } from './modifyTypes'
import { deleteItem, DeleteOptions } from './delete'
import { get, GetOptions, GetResult } from './get'
import { readFileSync } from 'fs'
import { join as pathJoin } from 'path'
import { Schema, SearchIndexes, Id } from './schema'
import { newSchemaDefinition } from './schema/updateSchema'
import { getSchema } from './schema/getSchema'
import getTypeFromId from './getTypeFromId'
import digest from './digest'
import { IdOptions } from '../lua/src/id'

const MAX_SCHEMA_UPDATE_RETRIES = 5

let SCRIPTS
try {
  SCRIPTS = ['modify', 'fetch', 'id', 'update-schema'].reduce(
    (obj, scriptName) => {
      return Object.assign(obj, {
        [scriptName]: readFileSync(
          pathJoin(process.cwd(), 'dist', 'lua', `${scriptName}.lua`),
          'utf8'
        )
      })
    },
    {}
  )
} catch (e) {
  console.error(`Failed to read modify.lua ${e.stack}`)
  process.exit(1)
}

export class SelvaClient {
  public schema: Schema
  public searchIndexes: SearchIndexes
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
      'id',
      SCRIPTS.id,
      0,
      [],
      [JSON.stringify(props)]
    )
  }

  async set(props: SetOptions) {
    return set(this, props)
  }

  async get(props: GetOptions) {
    return get(this, props)
  }

  async updateSchema(props: Schema, retry?: number) {
    retry = retry || 0

    const newSchema = newSchemaDefinition(this.schema, props)
    try {
      const updated = await this.redis.loadAndEvalScript(
        'update-schema',
        SCRIPTS['update-schema'],
        0,
        [],
        [JSON.stringify(newSchema)]
      )

      this.schema = updated
    } catch (e) {
      console.error('Error updating schema', e.stack)
      if (
        e.stack.includes(
          'SHA mismatch: trying to update an older schema version, please re-fetch and try again'
        )
      ) {
      }
      if (retry >= MAX_SCHEMA_UPDATE_RETRIES) {
        throw new Error(
          `Unable to update schema after ${MAX_SCHEMA_UPDATE_RETRIES} attempts`
        )
      }

      await this.getSchema()
      await this.updateSchema(props, retry + 1)
    }
  }

  async getSchema() {
    return getSchema(this)
  }

  async modify(opts: ModifyOptions): Promise<ModifyResult> {
    return this.redis.loadAndEvalScript(
      'modify',
      SCRIPTS.modify,
      0,
      [],
      [JSON.stringify(opts)],
      { batchingEnabled: true }
    )
  }

  async fetch(opts: GetOptions): Promise<GetResult> {
    const str = await this.redis.loadAndEvalScript(
      'fetch',
      SCRIPTS.fetch,
      0,
      [],
      [JSON.stringify(opts)]
    )

    return JSON.parse(str)
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
