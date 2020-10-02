import { SelvaClient } from '..'
import { GetResult, GetOptions, Sort } from './types'
import { SCRIPT } from '../constants'
import validate, {
  ExtraQueries,
  ExtraQuery,
  PostGetExtraQuery
} from './validate'
import { deepMerge } from './deepMerge'
import { FieldSchema, TypeSchema, Schema } from '~selva/schema'

async function combineResults(
  client: SelvaClient,
  extraQueries: ExtraQueries,
  $language: string | undefined,
  getResult: GetResult,
  meta?: any
) {
  if (Object.keys(extraQueries).length === 0) {
    return
  }

  if (Object.keys(getResult).length === 1 && getResult.listResult) {
    await Promise.all(
      getResult.listResult.map(res => {
        return combineResults(client, extraQueries, $language, res, meta)
      })
    )
    return
  }

  await Promise.all(
    Object.entries(extraQueries).map(async ([db, query]) => {
      await Promise.all(
        query.map(async q => {
          if (q.type === 'traverse' || q.type === 'text_search') {
            // these are processed before the main query
            if (meta) {
              deepMerge(meta, q.meta)
            }
            return
          }

          const parts = q.path.substr(1).split('.')

          if (parts[0] === 'listResult') {
            parts.shift()
          }

          let g = getResult
          for (let i = 0; i <= parts.length - 2; i++) {
            const part = parts[i]

            if (Array.isArray(g[part]) && isNaN(<any>parts[i + 1])) {
              const newQuery: ExtraQuery = {
                type: q.type,
                getOpts: q.getOpts,
                path: '.' + parts.slice(i + 1).join('.'),
                placeholder: q.placeholder
              }

              return Promise.all(
                g[part].map(r => {
                  return combineResults(
                    client,
                    { [db]: [newQuery] },
                    $language,
                    r,
                    meta
                  )
                })
              )
            }

            if (!g[part]) {
              g[part] = {}
            }

            g = g[part]
          }

          if (q.type === 'reference') {
            const r = await get(
              client,
              {
                $language,
                $id: g[parts[parts.length - 1]],
                $db: db,
                $includeMeta: !!meta,
                ...q.getOpts
              },
              meta,
              true
            )

            g[parts[parts.length - 1]] = r
          } else if (q.type === 'references') {
            if (q.getOpts.$list) {
              const { $db: _db, ...gopts } = q.getOpts
              const r = await get(
                client,
                {
                  $language,
                  $includeMeta: !!meta,
                  $db: db,
                  listResult: {
                    ...gopts,
                    $list: {
                      ...(gopts.$list === true ? {} : gopts.$list),
                      $find: {
                        ...(gopts.$list === true
                          ? {}
                          : gopts.$list.$find || {}),
                        $traverse: g[parts[parts.length - 1]]
                      }
                    }
                  }
                },
                meta,
                true
              )

              if (r.listResult[0] && r.listResult[0].__$find) {
                let fieldKeys = {}
                for (const key in q.getOpts) {
                  if (!key.startsWith('$') && key !== '__$find') {
                    fieldKeys[key] = q.getOpts[key]
                  }
                }

                const findOpts = r.listResult[0].__$find.opts
                const findIds = r.listResult.reduce((acc, e) => {
                  acc.add(...e.__$find.ids)
                  return acc
                }, new Set())
                const { $db: db, ...fopts } = findOpts
                const nestedResult = await get(
                  client,
                  {
                    $language,
                    $includeMeta: !!meta,
                    $db: db,
                    listResult: {
                      ...fieldKeys,
                      $list: {
                        $find: {
                          ...fopts,
                          $traverse: [...findIds]
                        }
                      }
                    }
                  },
                  meta,
                  true
                )

                g[parts[parts.length - 1]] = nestedResult.listResult
              } else {
                g[parts[parts.length - 1]] = r.listResult
              }
            } else if (q.getOpts.$find) {
              const { $db: _db, ...gopts } = q.getOpts
              const r = await get(
                client,
                {
                  $language,
                  $db: db,
                  $includeMeta: !!meta,
                  listResult: {
                    ...gopts,
                    $find: {
                      ...gopts.$find,
                      $traverse: g[parts[parts.length - 1]]
                    }
                  }
                },
                meta,
                true
              )
              g[parts[parts.length - 1]] = r.listResult
            }
          } else {
            const r = await get(
              client,
              {
                $language,
                $includeMeta: !!meta,
                $db: db,
                ...q.getOpts
              },
              meta,
              true
            )
            g[parts[parts.length - 1]] = r
          }
        })
      )
    })
  )
}

function getExtraQueriesByField(
  extraQueries: ExtraQueries
): Record<string, ExtraQuery> {
  const map: Record<string, ExtraQuery> = {}
  for (const db in extraQueries) {
    for (const q of extraQueries[db]) {
      map[q.path] = q
    }
  }

  return map
}

function makeNewGetOptions(
  extraQueries: Record<string, ExtraQuery>,
  getOpts: GetOptions,
  path: string = ''
): GetOptions {
  if (Object.keys(extraQueries).length === 0) {
    return getOpts
  }

  const newOpts = {}
  for (const key in getOpts) {
    const newPath = path + '.' + key
    if (extraQueries[newPath]) {
      const extraQuery: ExtraQuery = extraQueries[newPath]
      if (extraQuery.type === 'traverse') {
        newOpts[key] = extraQuery.value || []
      } else if (extraQuery.type === 'text_search') {
        // TODO: in full
        // here we gotta somehow convert the text search results into a $traverse and/or filter and/or something that makes sense in our new query
        // TODO: add $db support?
      } else {
        newOpts[key] = extraQuery.placeholder
      }
    } else if (
      !key.startsWith('$') &&
      key !== 'path' &&
      Array.isArray(getOpts[key])
    ) {
      newOpts[key] = getOpts[key].map((g, i) => {
        const extraQuery: PostGetExtraQuery = <PostGetExtraQuery>(
          extraQueries[newPath + '.' + i]
        )

        if (extraQuery) {
          return extraQuery.placeholder
        }

        return makeNewGetOptions(extraQueries, g, newPath + '.' + i)
      })
    } else if (Array.isArray(getOpts[key])) {
      newOpts[key] = getOpts[key]
    } else if (typeof getOpts[key] === 'object') {
      newOpts[key] = makeNewGetOptions(extraQueries, getOpts[key], newPath)
    } else {
      newOpts[key] = getOpts[key]
    }
  }

  return newOpts
}

async function resolveId(
  client: SelvaClient,
  props: GetOptions
): Promise<string | undefined> {
  if (props.$id) {
    if (Array.isArray(props.$id)) {
      const exists: boolean[] = await Promise.all(
        props.$id.map(id => {
          return client.redis.exists({ name: props.$db || 'default' }, id)
        })
      )

      const idx = exists.findIndex(x => !!x)
      if (idx === -1) {
        return null
      }

      return props.$id[idx]
    } else {
      return props.$id
    }
  } else if (props.$alias) {
    const alias = Array.isArray(props.$alias) ? props.$alias : [props.$alias]
    const resolved: [string | null, boolean][] = <[string | null, boolean][]>(
      await Promise.all(
        alias.map(async alias => {
          return [
            await client.redis.hget(
              { name: props.$db || 'default' },
              '___selva_aliases',
              alias
            ),
            await client.redis.exists({ name: props.$db || 'default' }, alias)
          ]
        })
      )
    )

    const idx = resolved.findIndex(x => {
      return !!x[0] || !!x[1]
    })

    if (idx === -1) {
      return null
    }

    return resolved[idx][0] || alias[idx]
  } else {
    return 'root'
  }
}

type GetOp =
  | {
      type: 'db'
      id: string
      field: string
      sourceField: string | string[]
      default?: any
    }
  | { type: 'value'; value: string; field: string }
  | { type: 'nested_query'; props: GetOptions; field: string }
  | { type: 'array_query'; props: GetOptions[]; field: string; id: string }
  | {
      type: 'find'
      props: GetOptions
      field: string
      sourceField: string | string[]
      id: string
      options: { limit: number; offset: number; sort?: Sort | undefined }
    }

function _thing(
  ops: GetOp[],
  client: SelvaClient,
  props: GetOptions,
  id: string,
  field: string
): void {
  if (props.$value) {
    ops.push({
      type: 'value',
      field: field.substr(1),
      value: props.$value
    })
  } else if (props.$id && field) {
    ops.push({
      type: 'nested_query',
      field: field.substr(1),
      props
    })
  } else if (Array.isArray(props)) {
    ops.push({
      type: 'array_query',
      id,
      field: field.substr(1),
      props
    })
  } else if (props.$list) {
    if (props.$list === true) {
      ops.push({
        type: 'db',
        id,
        field: field.substr(1),
        sourceField: <string[]>props.$field || field.substr(1)
      })
    } else if (props.$list.$find) {
      // TODO: $find in $list
    } else {
      ops.push({
        type: 'find',
        id,
        props,
        field: field.substr(1),
        sourceField: <string>props.$field || field.substr(1),
        options: {
          limit: props.$list.$limit || -1,
          offset: props.$list.$offset || 0,
          sort: Array.isArray(props.$list.$sort)
            ? props.$list.$sort[0]
            : props.$list.$sort || undefined
        }
      })
    }
  } else if (props.$find) {
    // TODO
  } else if (
    props.$field &&
    typeof props.$field === 'object' &&
    (<any>props.$field).value
  ) {
    // TODO
  } else if (props.$field) {
    ops.push({
      type: 'db',
      id,
      field: field.substr(1),
      sourceField: <string[]>props.$field
    })
  } else if (props.$all) {
    // TODO: proper $db support, well, everywhere basically not just here
    const schema = client.schemas.default
    if (field === '') {
      const type = getTypeFromId(schema, id)
      const typeSchema = type === 'root' ? schema.rootType : schema.types[type]

      if (!typeSchema) {
        return
      }

      for (const key in typeSchema.fields) {
        if (
          key !== 'children' &&
          key !== 'parents' &&
          key !== 'ancestors' &&
          key !== 'descendants'
        ) {
          if (props[key] === undefined) {
            ops.push({
              type: 'db',
              id,
              field: key,
              sourceField: key
            })
          } else if (props[key] === false) {
            // do nothing
          } else {
            _thing(ops, client, props[key], id, field + '.' + key)
          }
        }
      }

      return
    }

    const fieldSchema = getNestedSchema(schema, id, field.slice(1))
    if (!fieldSchema) {
      return
    }

    if (fieldSchema.type === 'object') {
      for (const key in fieldSchema.properties) {
        if (props[key] === undefined) {
          ops.push({
            type: 'db',
            id,
            field: field.slice(1) + '.' + key,
            sourceField: field.slice(1) + '.' + key
          })
        } else if (props[key] === false) {
          // do nothing
        } else {
          _thing(ops, client, props[key], id, field + '.' + key)
        }
      }
    } else if (fieldSchema.type === 'record' || fieldSchema.type === 'text') {
      // basically this is the same as: `field: true`
      ops.push({
        type: 'db',
        id,
        field: field.slice(1),
        sourceField: field.slice(1)
      })
    }
  } else if (props.$default) {
    ops.push({
      type: 'db',
      id,
      field: field.substr(1),
      sourceField: field.substr(1),
      default: props.$default
    })
  } else if (typeof props === 'object') {
    for (const key in props) {
      if (key.startsWith('$')) {
        continue
      }

      _thing(ops, client, props[key], id, field + '.' + key)
    }
  } else {
    ops.push({
      type: 'db',
      id,
      field: field.substr(1),
      sourceField: field.substr(1)
    })
  }
}

export const setNestedResult = (
  result: GetResult,
  field: string,
  value: any
) => {
  if (!field) {
    return
  }

  if (field === '') {
    for (const k in value) {
      result[k] = value[k]
    }

    return
  }

  const fields = field.split('.')
  const len = fields.length
  if (len > 1) {
    let segment = result
    for (let i = 0; i < len - 1; i++) {
      segment = segment[fields[i]] || (segment[fields[i]] = {})
    }
    segment[fields[len - 1]] = value
  } else {
    result[field] = value
  }
}

function getTypeFromId(schema: Schema, id: string): string | undefined {
  if (id.startsWith('ro')) {
    return 'root'
  }

  return schema.prefixToTypeMapping[id.substr(0, 2)]
}

export function getNestedSchema(
  schema: Schema,
  id: string,
  field: string
): FieldSchema | null {
  if (!field || field === '') {
    return null
  }

  const type = getTypeFromId(schema, id)
  const fields = field.split('.')

  const typeSchema = type === 'root' ? schema.rootType : schema.types[type]
  if (!typeSchema || !typeSchema.fields) {
    return null
  }

  let prop: any = typeSchema.fields[fields[0]]
  if (!prop) {
    return null
  }

  for (let i = 1; i < fields.length; i++) {
    const segment = fields[i]

    if (!prop) {
      return null
    }

    if (prop.type === 'text' && i === fields.length - 1) {
      return { type: 'string' }
    }

    if (prop.values) {
      // record types skip the next key
      prop = prop.values
    } else {
      if (!prop.properties) {
        return null
      }

      prop = prop.properties[segment]
    }
  }

  return prop
}

const TYPE_TO_SPECIAL_OP: Record<
  string,
  (
    client: SelvaClient,
    id: string,
    field: string,
    _lang?: string
  ) => Promise<any>
> = {
  id: async (
    client: SelvaClient,
    id: string,
    field: string,
    _lang?: string
  ) => {
    return id
  },
  references: async (
    client: SelvaClient,
    id: string,
    field: string,
    _lang?: string
  ) => {
    const paddedId = id.padEnd(10, '\0')

    if (field === 'ancestors') {
      return client.redis.selva_hierarchy_find(
        '___selva_hierarchy',
        'bfs',
        'ancestors',
        paddedId
      )
    } else if (field === 'descendants') {
      return client.redis.selva_hierarchy_find(
        '___selva_hierarchy',
        'bfs',
        'descendants',
        paddedId
      )
    } else if (field === 'parents') {
      return client.redis.selva_hierarchy_parents('___selva_hierarchy', id)
    } else if (field === 'children') {
      return client.redis.selva_hierarchy_children('___selva_hierarchy', id)
    } else {
      return client.redis.zrange(id + '.' + field, 0, -1)
    }
  },
  set: async (
    client: SelvaClient,
    id: string,
    field: string,
    _lang?: string
  ) => {
    const set = await client.redis.zrange(id + '.' + field, 0, -1)
    if (set && set.length) {
      return set
    }
  },
  text: async (
    client: SelvaClient,
    id: string,
    field: string,
    lang?: string
  ) => {
    const all = await client.redis.hgetall(id)
    const result: any = {}
    let hasFields = false
    Object.entries(all).forEach(([key, val]) => {
      if (key.startsWith(field + '.')) {
        hasFields = true
        setNestedResult(result, key.slice(field.length + 1), val)
      }
    })

    if (lang) {
      if (result[lang]) {
        return result[lang]
      }

      const allLangs = client.schemas.default.languages
      for (const l of allLangs) {
        if (result[l]) {
          return result[l]
        }
      }
    }

    if (hasFields) {
      return result
    }
  },
  object: async (
    client: SelvaClient,
    id: string,
    field: string,
    _lang?: string
  ) => {
    const all = await client.redis.hgetall(id)
    const result: any = {}
    let hasKeys = false
    await Promise.all(
      Object.entries(all).map(async ([key, val]) => {
        if (key.startsWith(field + '.')) {
          hasKeys = true

          if (val === '___selva_$set') {
            const set = await client.redis.zrange(id + '.' + field, 0, -1)
            if (set) {
              setNestedResult(result, key.slice(field.length + 1), set)
            }
            return
          }

          const fieldSchema = getNestedSchema(client.schemas.default, id, key)
          const typeCast = TYPE_CASTS[fieldSchema.type]
          if (typeCast) {
            val = typeCast(val)
          }
          setNestedResult(result, key.slice(field.length + 1), val)
        }
      })
    )

    if (hasKeys) {
      return result
    }
  },
  record: async (
    client: SelvaClient,
    id: string,
    field: string,
    _lang: string
  ) => {
    return TYPE_TO_SPECIAL_OP.object(client, id, field, _lang)
  }
}

const TYPE_CASTS: Record<string, (x: any) => any> = {
  float: Number,
  number: Number,
  int: Number,
  boolean: (x: any) => (x === '0' ? false : true),
  json: (x: any) => JSON.parse(x),
  array: (x: any) => JSON.parse(x)
}

async function getThings(
  client: SelvaClient,
  lang: string | undefined,
  ops: GetOp[]
): Promise<GetResult> {
  const results = await Promise.all(
    ops.map(async op => {
      if (op.type === 'value') {
        return op.value
      } else if (op.type === 'nested_query') {
        return run(client, op.props)
      } else if (op.type === 'array_query') {
        return Promise.all(
          op.props.map(p => {
            if (p.$id) {
              return run(client, p)
            } else {
              return run(client, Object.assign({}, p, { $id: op.id }))
            }
          })
        )
      } else if (op.type === 'find') {
        console.log('OP', op)
        const ids = await client.redis.selva_hierarchy_find(
          '___selva_hierarchy',
          'bfs',
          <string>op.sourceField, // TODO: this needs to support sourceField I think?
          'order',
          op.options.sort.$field || '',
          op.options.sort.$order || 'asc',
          'offset',
          op.options.offset,
          'limit',
          op.options.limit,
          op.id.padEnd(10, '\0'),
          '#1'
        )

        console.log('IDS', ids)

        return Promise.all(
          ids.map(id => {
            const realOpts: any = {}
            for (const key in op.props) {
              if (!key.startsWith('$')) {
                realOpts[key] = op.props[key]
              }
            }

            return run(client, {
              $id: id,
              ...realOpts
            })
          })
        )
      }

      // op.type === 'db'

      let r: any
      let fieldSchema
      if (Array.isArray(op.sourceField)) {
        fieldSchema = getNestedSchema(
          client.schemas.default,
          op.id,
          op.sourceField[0]
        )

        if (!fieldSchema) {
          return null
        }

        const specialOp = TYPE_TO_SPECIAL_OP[fieldSchema.type]

        const nested: GetOp[] = await Promise.all(
          op.sourceField.map(f => {
            if (specialOp) {
              return specialOp(client, op.id, f, lang)
            }

            return client.redis.hget(op.id, f)
          })
        )

        r = nested.find(x => !!x)
      } else {
        fieldSchema = getNestedSchema(
          client.schemas.default,
          op.id,
          op.sourceField
        )

        if (!fieldSchema) {
          return null
        }

        const specialOp = TYPE_TO_SPECIAL_OP[fieldSchema.type]
        if (specialOp) {
          r = await specialOp(client, op.id, op.sourceField, lang)
        } else {
          r = await client.redis.hget(op.id, op.sourceField)
        }
      }

      if (r !== null && r !== undefined) {
        const typeCast = TYPE_CASTS[fieldSchema.type]
        if (typeCast) {
          return typeCast(r)
        }

        return r
      } else if (op.default) {
        return op.default
      }
    })
  )

  const o: GetResult = {}
  results.map((r, i) => {
    if (r !== null && r !== undefined) {
      setNestedResult(o, ops[i].field, r)
    }
  })

  return o
}

async function run(client: SelvaClient, props: GetOptions): Promise<GetResult> {
  const id = await resolveId(client, props)

  if (!id) {
    return { $isNull: true }
  }

  const lang = props.$language

  const things: any[] = []
  _thing(things, client, props, id, '')
  return getThings(client, lang, things)
}

async function get(
  client: SelvaClient,
  props: GetOptions,
  meta?: any,
  nested: boolean = false
): Promise<GetResult> {
  const extraQueries: ExtraQueries = {}
  await validate(extraQueries, client, props)
  const newProps = makeNewGetOptions(
    getExtraQueriesByField(extraQueries),
    props
  )

  const getResult = await run(client, props)
  if (meta || props.$includeMeta) {
    if (!meta) {
      if (!getResult.$meta) {
        getResult.$meta = {}
      }
      meta = { [props.$db || 'default']: getResult.$meta }
      meta.___refreshAt = getResult.$meta.___refreshAt
    } else {
      if (getResult.$meta.___refreshAt) {
        if (
          !meta.___refreshAt ||
          meta.___refreshAt > getResult.$meta.___refreshAt
        ) {
          meta.___refreshAt = getResult.$meta.___refreshAt
        }
      }

      deepMerge(meta, {
        [props.$db || 'default']: getResult.$meta
      })

      delete getResult.$meta
    }
  }

  await combineResults(client, extraQueries, props.$language, getResult, meta)

  if (props.$includeMeta && !nested) {
    getResult.$meta = meta
  }

  return getResult
}

export { get, GetResult, GetOptions }
