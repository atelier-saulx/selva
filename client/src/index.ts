import { default as RedisClient, ConnectOptions } from './redis'
import { set, SetOptions } from './set'
import { ModifyOptions, ModifyResult } from './modifyTypes'
import { deleteItem, DeleteOptions } from './delete'
import { get, GetOptions, GetResult } from './get'
import { observe } from './observe/index'
import Observable from './observe/observable'
import { readFileSync } from 'fs'
import { EventEmitter } from 'events'
import { join as pathJoin } from 'path'
import {
  Schema,
  SearchIndexes,
  SchemaOptions,
  Id,
  FieldSchemaObject,
  GetSchemaResult
} from './schema'
import { newSchemaDefinition } from './schema/updateSchema'
import { getSchema } from './schema/getSchema'
import getTypeFromId from './getTypeFromId'
import digest from './digest'
import { IdOptions } from '../lua/src/id'
import { v4 as uuid } from 'uuid'
const MAX_SCHEMA_UPDATE_RETRIES = 100
export type LogEntry = { level: LogLevel; msg: string }
export type LogLevel = 'info' | 'notice' | 'warning' | 'error' | 'off'
export type LogFn = (log: LogEntry) => void

export { default as prefixes } from './prefixes'

export type SelvaOptions = {
  loglevel?: LogLevel
  log?: LogFn
}

const wait = (t: number = 0): Promise<void> =>
  new Promise(r => setTimeout(r, t))

// split this up and clean it!

let SCRIPTS

try {
  SCRIPTS = ['modify', 'fetch', 'id', 'update-schema'].reduce(
    (obj, scriptName) => {
      let distPath = pathJoin(__dirname, '..')
      if (!distPath.endsWith('dist')) {
        distPath = pathJoin(distPath, 'dist')
      }
      return Object.assign(obj, {
        [scriptName]: readFileSync(
          pathJoin(distPath, 'lua', `${scriptName}.lua`),
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

export class SelvaClient extends EventEmitter {
  public schema: Schema
  public searchIndexes: SearchIndexes
  public redis: RedisClient
  private loglevel: LogLevel = 'off'
  public clientId: string
  private schemaObservable: Observable<Schema>

  constructor(
    opts:
      | ConnectOptions
      | (() => Promise<ConnectOptions>)
      | Promise<ConnectOptions>,
    selvaOpts?: SelvaOptions
  ) {
    super()
    this.clientId = uuid()
    if (selvaOpts && selvaOpts.loglevel) {
      this.loglevel = selvaOpts.loglevel
    } else {
      this.loglevel = 'off'
      if (!selvaOpts) {
        selvaOpts = {}
      }
      selvaOpts.loglevel = 'off'
    }

    this.setMaxListeners(100)
    this.redis = new RedisClient(opts, this, selvaOpts)
  }

  subscribeSchema() {
    console.log('SUBSCRIBE SCHEMA')
    if (this.schemaObservable) {
      return this.schemaObservable
    }

    const obs = this.redis.subscribe(`___selva_subscription:schema_update`, {})

    this.schemaObservable = new Observable<Schema>(observe => {
      const sub = obs.subscribe({
        next: (_x: any) => {
          observe.next(_x)
        },
        error: observe.error,
        complete: observe.complete
      })

      return <any>sub
    })

    return this.schemaObservable
  }

  async conformToSchema(props: SetOptions): Promise<SetOptions> {
    if (!props.$id && !props.type && !props.$alias) {
      return null
    }

    if (props.$id !== 'root') {
      if (!props.type) {
        if (props.$id) {
          props.type = await getTypeFromId(this, props.$id)
        } else {
          const typePayload = await this.get({
            $alias: props.$alias,
            type: true,
            id: true
          })

          props.type = typePayload.type
          props.$id = typePayload.id
        }
      }

      if (!props.type) {
        return null
      }
    }

    const typeSchema =
      props.$id === 'root'
        ? this.schema.rootType
        : this.schema.types[props.type]

    const newProps: SetOptions = {
      type: props.type
    }

    if (props.$id) {
      newProps.$id = props.$id
    }

    if (props.$alias) {
      newProps.$alias = props.$alias
    }

    const mergeObject: (
      x: SetOptions,
      schema: FieldSchemaObject
    ) => SetOptions = (oldObj: SetOptions, schema: FieldSchemaObject) => {
      const newObj: SetOptions = {}
      for (const key in oldObj) {
        if (schema.properties[key]) {
          if (schema.properties[key].type === 'object') {
            newObj[key] = mergeObject(
              oldObj[key],
              <FieldSchemaObject>schema.properties[key]
            )
          } else if (
            schema.properties[key].type === 'array' &&
            // @ts-ignore
            schema.properties[key].items.type === 'object'
          ) {
            newObj[key] = oldObj[key].map(x => {
              // @ts-ignore
              return mergeObject(x, schema.properties[key].items)
            })
          } else if (
            schema.properties[key].type === 'set' &&
            // @ts-ignore
            schema.properties[key].items.type === 'object'
          ) {
            newObj[key] = oldObj[key].map(x => {
              // @ts-ignore
              return mergeObject(x, schema.properties[key].items)
            })
          } else {
            newObj[key] = oldObj[key]
          }
        }
      }

      return newObj
    }

    for (const key in props) {
      if (typeSchema.fields[key]) {
        if (typeSchema.fields[key].type === 'object') {
          newProps[key] = mergeObject(
            props[key],
            <FieldSchemaObject>typeSchema.fields[key]
          )
        } else if (
          typeSchema.fields[key].type === 'array' &&
          // @ts-ignore
          typeSchema.fields[key].items.type === 'object'
        ) {
          newProps[key] = props[key].map(x => {
            // @ts-ignore
            return mergeObject(x, typeSchema.fields[key].items)
          })
        } else if (
          typeSchema.fields[key].type === 'set' &&
          // @ts-ignore
          typeSchema.fields[key].items.type === 'object'
        ) {
          newProps[key] = props[key].map(x => {
            // @ts-ignore
            return mergeObject(x, typeSchema.fields[key].items)
          })
        } else {
          newProps[key] = props[key]
        }
      }
    }

    return newProps
  }

  digest(payload: string) {
    return digest(payload)
  }

  async destroy() {
    this.removeAllListeners()
    this.redis.destroy()
  }

  async id(props: IdOptions): Promise<string> {
    // move to js
    return this.redis.loadAndEvalScript(
      'id',
      SCRIPTS.id,
      0,
      [],
      [JSON.stringify(props)],
      'client'
    )
  }

  async set(props: SetOptions) {
    return set(this, props)
  }

  async get(props: GetOptions) {
    return get(this, props)
  }

  observe(props: GetOptions) {
    return observe(this, props)
  }

  async updateSchema(props: SchemaOptions, retry?: number) {
    retry = retry || 0
    if (!props.types) {
      props.types = {}
    }
    const newSchema = newSchemaDefinition(this.schema, <Schema>props)
    try {
      const updated = await this.redis.loadAndEvalScript(
        'update-schema',
        SCRIPTS['update-schema'],
        0,
        [],
        [`${this.loglevel}:${this.clientId}`, JSON.stringify(newSchema)],
        'client'
      )
      if (updated) {
        this.schema = JSON.parse(updated)
      }
    } catch (e) {
      if (
        e.stack.includes(
          'SHA mismatch: trying to update an older schema version, please re-fetch and try again'
        )
      ) {
        if (retry >= MAX_SCHEMA_UPDATE_RETRIES) {
          throw new Error(
            `Unable to update schema after ${MAX_SCHEMA_UPDATE_RETRIES} attempts`
          )
        }
        await this.getSchema()
        await wait(retry * 200)
        await this.updateSchema(props, retry + 1)
      } else {
        if (e.code === 'NR_CLOSED') {
          // canhappen with load and eval script
        } else {
          throw e
        }
      }
    }
  }

  async getSchema() {
    return getSchema(this)
  }

  async modify(opts: ModifyOptions, retry: number = 0): Promise<ModifyResult> {
    if (!this.schema || !this.schema.sha) {
      await this.getSchema()
    }

    try {
      return await this.redis.loadAndEvalScript(
        'modify',
        SCRIPTS.modify,
        0,
        [],
        [
          `${this.loglevel}:${this.clientId}`,
          this.schema.sha,
          JSON.stringify(opts)
        ],
        'client',
        { batchingEnabled: true }
      )
    } catch (e) {
      if (
        e.stack &&
        e.stack.includes(
          'SHA mismatch: trying to update an older schema version, please re-fetch and try again'
        )
      ) {
        if (retry >= MAX_SCHEMA_UPDATE_RETRIES) {
          throw new Error(
            `Unable to update schema after ${MAX_SCHEMA_UPDATE_RETRIES} attempts`
          )
        }

        await this.getSchema()
        await this.modify(opts, retry + 1)
      } else {
        throw e
      }
    }
  }

  async fetch(opts: GetOptions): Promise<GetResult> {
    const str = await this.redis.loadAndEvalScript(
      'fetch',
      SCRIPTS.fetch,
      0,
      [],
      [`${this.loglevel}:${this.clientId}`, JSON.stringify(opts)],
      'client'
      // { batchingEnabled: true } prob needs this?
    )

    return JSON.parse(str)
  }

  async getTypeFromId(id: Id) {
    return getTypeFromId(this, id)
  }

  async delete(props: DeleteOptions) {
    return deleteItem(this, props)
  }
}

export function connect(
  opts:
    | ConnectOptions
    | (() => Promise<ConnectOptions>)
    | Promise<ConnectOptions>,
  selvaOpts?: SelvaOptions
): SelvaClient {
  const client = new SelvaClient(opts, selvaOpts)
  return client
}

export * from './schema/index'
export * from './get/types'
export { ConnectOptions } from './redis'
