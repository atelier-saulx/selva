import { SelvaClient, SetOptions, Schema } from '..'
import createFindOperation from '../get/createGetOperations/find'
import { padId } from '../util'
import { ast2rpn } from '@saulx/selva-query-ast-parser'
import parseSetObject from '../set/validate'

export type UpdateQuery = {
  $find: any
  $id?: string
  $alias?: string | string[]
}

async function update(
  client: SelvaClient,
  payload: SetOptions,
  query: UpdateQuery,
  schema?: Schema
): Promise<boolean | void> {
  if (!schema) {
    schema = client.schemas[payload.$db || 'default']
  }

  if (!query.$id && query.$alias) {
    let aliases: string[]
    if (!Array.isArray(query.$alias)) {
      aliases = [query.$alias]
    } else {
      aliases = query.$alias
    }

    for (const alias of aliases) {
      const id = await client.redis.hget(`___selva_aliases`, alias)
      if (id) {
        payload.$id = id
        break
      }
    }

    if (!query.$id) {
      throw new Error(
        `.batchedSet() without the type property requires an existing record or $id to be set with the wanted type prefix. No existing id found for alias ${JSON.stringify(
          query.$alias
        )}`
      )
    }
  }

  const op = createFindOperation(
    client,
    payload.$db || 'default',
    query.$find,
    {},
    payload.$id || 'root',
    '',
    false
  )

  const args = op.filter ? ast2rpn(schema.types, op.filter) : []

  const parsed = await parseSetObject(client, payload, schema, undefined, true)

  // const b = [
  //   { name: payload.$db || 'default' },
  //   '___selva_hierarchy',
  //   query.$find.$traverse,
  //   (parsed.length - 1) / 3,
  //   ...parsed.slice(1),
  //   padId(op.id),
  //   ...args,
  // ]

  const x = await client.redis.selva_update(
    { name: payload.$db || 'default' },
    '___selva_hierarchy',
    query.$find.$traverse,
    (parsed.length - 1) / 3,
    ...parsed.slice(1),
    padId(op.id),
    ...args
  )

  console.info(x)
}

export { update }
