import { SetOptions, BatchOpts, BatchRefFieldOpts } from './types'
import { SelvaClient } from '..'
import { Schema, TypeSchema } from '../schema'
import fieldParsers from './fieldParsers'
import { verifiers } from './fieldParsers/simple'
import { v4 as uuid } from 'uuid'
import { SCRIPT } from '../constants'

// import { MAX_BATCH_SIZE } from '../redis'
const MAX_BATCH_SIZE = 5500

const ALLOWED_OPTIONS_DOCS = `
Record identification (if neither $id or $alias is provided, 'root' id is assumed)
- $id 
- $alias
General set operators
- $language: string (optional) (used to automatically fetch specified language from 'text' type fields and properties)
- $merge: boolean (optional) (whether set fields are merged to existing record fields, or set to override)
- $version: string (optional) TODO: version is not functional yet, coming soon
`

// TODO: add link to set payload by type
function allowedFieldsDoc(schemas: Schema, type?: string): string {
  let typeSchema: TypeSchema
  if (type) {
    typeSchema = schemas.types[type]
  }

  if (typeSchema) {
    let str = ''
    for (const key in typeSchema.fields) {
      str += `        - ${key}: ${typeSchema.fields[key].type} \n`
    }

    return str
  }

  return ''
}

export const parseSetObject = (
  payload: SetOptions,
  schemas: Schema,
  $lang?: string
): SetOptions => {
  const result: SetOptions = {}

  if (!payload.type && schemas.prefixToTypeMapping && payload.$id) {
    payload.type = schemas.prefixToTypeMapping[payload.$id.substring(0, 2)]
  }

  if (!payload.type && payload.$id === 'root') {
    payload.type = 'root'
  }

  if (payload.$language) {
    $lang = payload.$language
  }
  const type = payload.type

  const schema = type === 'root' ? schemas.rootType : schemas.types[type]
  if (!schema) {
    throw new Error(
      `Cannot find type ${type ||
        ` from prefix ${payload.$id.substring(
          0,
          2
        )}`} from set-object ${JSON.stringify(payload, null, 2)}`
    )
  }

  let fields = schema.fields
  for (let key in payload) {
    if (key[0] === '$') {
      if (key === '$merge') {
        if (!(payload[key] === true || payload[key] === false)) {
          throw new Error(`$merge needs to be a a boolean `)
        }
        result[key] = payload[key]
      } else if (key === '$id') {
        if (!verifiers.id(payload[key])) {
          throw new Error('Wrong type for $id ' + payload[key])
        }
        result[key] = payload[key]
      } else if (key === '$operation') {
        const val = payload[key]
        if (val !== 'update' && val !== 'insert' && val !== 'upsert') {
          throw new Error('Wrong type for $operation ' + payload[key])
        }

        result[key] = payload[key]
      } else if (key === '$source') {
        if (
          typeof payload[key] !== 'string' &&
          typeof payload[key] !== 'object'
        ) {
          throw new Error('Wrong type for $source, string or object required')
        }

        result[key] = payload[key]
      } else if (key === '$alias') {
        if (typeof payload[key] !== 'string' && !Array.isArray(payload[key])) {
          throw new Error('Wrong type for $alias, string or array required')
        }

        result[key] = payload[key]
      } else if (key === '$_batchOpts') {
        // internally used
        result[key] = payload[key]
      } else if (key === '$version') {
        if (typeof payload[key] !== 'string') {
          throw new Error('Wrong type for $version, string required')
        }
        console.warn('$version is not implemented yet!')
        result[key] = payload[key]
      } else if (key === '$language') {
        if (
          typeof payload[key] !== 'string' ||
          String(payload[key]).length > 100
        ) {
          throw new Error(
            `Wrong type for language ${payload[key]}, string required`
          )
        }

        if (
          schemas &&
          schemas.languages &&
          !schemas.languages.includes(payload[key])
        ) {
          throw new Error(
            `Wrong value for language ${
              payload[key]
            }, schema allows the following languages to be set: ${schemas.languages.join(
              ', '
            )}`
          )
        }
      } else if (key === '$_itemCount') {
        // ignore
        result[key] = payload[key]
      } else {
        throw new Error(`Unsupported operator on set ${key}. Did you mean one following set operators?
          ${ALLOWED_OPTIONS_DOCS}`)
      }
    } else if (!fields[key]) {
      throw new Error(`
        Cannot find field ${key} in ${type}. Did you mean one of the following properties?
${allowedFieldsDoc(schemas, type)}
        `)
    } else {
      const fn = fieldParsers[fields[key].type]
      fn(schemas, key, payload[key], result, fields[key], type, $lang)
    }
  }
  return result
}

async function _set(
  client: SelvaClient,
  payload: SetOptions,
  schemaSha: string,
  db?: string
): Promise<string> {
  const res = await client.redis.evalsha(
    { name: db || 'default' },
    `${SCRIPT}:modify`,
    0,
    `undefined:undefined`,
    schemaSha,
    JSON.stringify({
      kind: 'update',
      payload
    })
  )

  return res[0]
}

async function set(client: SelvaClient, payload: SetOptions): Promise<string> {
  const schemaResp = await client.getSchema(payload.$db)
  const schema = schemaResp.schema
  const parsed = parseSetObject(payload, schema)

  if (parsed.$_itemCount > MAX_BATCH_SIZE) {
    const [id] = await setInBatches(schema, client, parsed, 0, {
      $_batchOpts: { batchId: uuid() },
      db: payload.$db
    })
    return id
  }

  return _set(client, parsed, schema.sha, payload.$db)
}

async function setInBatches(
  schemas: Schema,
  client: SelvaClient,
  payload: SetOptions | SetOptions[],
  depth: number,
  context?: {
    db?: string
    id?: string
    alias?: string | string[]
    field?: string
    add?: boolean
    $_batchOpts?: BatchOpts
  }
): Promise<string[]> {
  if (Array.isArray(payload)) {
    const { $_batchOpts, field, add, id } = context
    // context is always defined here
    // const BATCH_SIZE = MAX_BATCH_SIZE - 10 // allow for some operations to be batched with these
    const BATCH_SIZE = MAX_BATCH_SIZE
    const allIds: string[] = []
    let idx = 0

    do {
      const slice = payload.slice(idx, idx + BATCH_SIZE)

      const entries = await Promise.all(
        slice.map(async i => {
          if (typeof i === 'string' || i.$id || i.$alias) {
            return i
          }

          const id = await client.id({
            type: i.type
          })

          i.$id = id
          return i
        })
      )

      const refFieldOpts: BatchRefFieldOpts = {
        last: idx + BATCH_SIZE >= payload.length
      }

      if (!add) {
        refFieldOpts.resetReference = field
      }

      const opts: SetOptions =
        add || idx > 0
          ? {
              $_batchOpts: Object.assign({}, $_batchOpts, {
                refField: refFieldOpts
              }),
              $id: id,
              [field]: { $add: entries }
            }
          : {
              $_batchOpts: Object.assign({}, $_batchOpts, {
                refField: refFieldOpts
              }),
              $id: id,
              [field]: entries
            }

      await _set(client, opts, schemas.sha, context.db)

      const ids = entries.map(e => {
        if (typeof e === 'string') {
          return e
        }

        return e.$id
      })

      allIds.push(...(<string[]>ids))
      idx += BATCH_SIZE
    } while (idx < payload.length)

    return allIds
  }

  if (payload.$_itemCount < MAX_BATCH_SIZE && payload.$_itemCount !== 0) {
    const id = await _set(client, payload, schemas.sha, context.db)

    payload.$id = <string>id
    payload.$_itemCount = 0
    return [<string>id]
  }

  const db: string | undefined = payload.$db || context.db || 'default'
  const { $_batchOpts } = context
  const size = payload.$_itemCount
  let fieldNames: string[] = []
  let missingFieldNames: string[] = []
  let batchQueue: SetOptions[] = []
  let remainingBatchSize = MAX_BATCH_SIZE
  for (const field in payload) {
    if (payload[field].$_itemCount) {
      if (remainingBatchSize - payload[field].$_itemCount >= 0) {
        remainingBatchSize -= payload[field].$_itemCount
        batchQueue.push(payload[field])
        fieldNames.push(field)
      } else if (payload[field].$_itemCount > MAX_BATCH_SIZE) {
        if (!payload.$id && !payload.$alias) {
          payload.$id = await client.id({
            type: payload.type
          })
          payload.$id = undefined
        }

        if (payload[field].$add) {
          await setInBatches(schemas, client, payload[field].$add, depth + 1, {
            db,
            id: payload.$id,
            alias: payload.$alias,
            field,
            add: true,
            $_batchOpts
          })
        } else if (Array.isArray(payload[field])) {
          await setInBatches(schemas, client, payload[field], depth + 1, {
            db,
            id: payload.$id,
            alias: payload.$alias,
            field,
            add: false,
            $_batchOpts
          })
        } else {
          fieldNames.push(field)
        }
      } else {
        missingFieldNames.push(field)
      }
    } else {
      fieldNames.push(field)
    }
  }

  const newPayload: SetOptions = { $_batchOpts }
  for (const field of fieldNames) {
    newPayload[field] = payload[field]
  }

  const missingPayload: SetOptions = {}
  let missingFieldsCount = 0
  for (const field of missingFieldNames) {
    missingFieldsCount++
    missingPayload[field] = payload[field]
  }

  const isLast = depth === 0 && missingFieldsCount === 0
  newPayload.$_batchOpts = {
    batchId: $_batchOpts.batchId,
    last: isLast
  }

  newPayload.$_itemCount = MAX_BATCH_SIZE - remainingBatchSize
  const id = await _set(client, newPayload, schemas.sha, db)

  if (missingFieldsCount) {
    missingPayload.$id = <string>id
    missingPayload.$_itemCount = size - newPayload.$_itemCount
    await setInBatches(schemas, client, missingPayload, depth, {
      $_batchOpts,
      db
    })
  }

  return [<string>id]
}

export { set, SetOptions }
