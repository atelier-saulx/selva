import { SelvaClient } from '../'
import { GetOperation, GetResult } from './types'
import { setNestedResult, getNestedSchema } from './utils'
import resolveId from './resolveId'
import createGetOperations from './createGetOperations'
import { GetOptions } from '.'
import { ast2rpn } from '@saulx/selva-query-ast-parser'

const TYPE_CASTS: Record<string, (x: any) => any> = {
  float: Number,
  number: Number,
  int: Number,
  boolean: (x: any) => (x === '0' ? false : true),
  json: (x: any) => JSON.parse(x),
  array: (x: any) => JSON.parse(x)
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

const executeNestedGetOperations = async (
  client: SelvaClient,
  props: GetOptions,
  lang: string | undefined,
  db: string
): Promise<GetResult> => {
  const id = await resolveId(client, props)
  return await executeGetOperations(
    client,
    props.$language || lang,
    db,
    createGetOperations(client, props, id, '', db)
  )
}

export default async function executeGetOperations(
  client: SelvaClient,
  lang: string | undefined,
  db: string,
  ops: GetOperation[]
): Promise<GetResult> {
  const myBlurx = async op => {
    if (op.type === 'value') {
      return op.value
    } else if (op.type === 'nested_query') {
      return executeNestedGetOperations(client, op.props, lang, db)
    } else if (op.type === 'array_query') {
      return Promise.all(
        op.props.map(p => {
          if (p.$id) {
            return executeNestedGetOperations(client, p, lang, db)
          } else {
            return executeNestedGetOperations(
              client,
              Object.assign({}, p, { $id: op.id }),
              lang,
              db
            )
          }
        })
      )
    } else if (op.type === 'find') {
      let sourceField: string = <string>op.sourceField
      if (Array.isArray(op.sourceField)) {
        const exists = await Promise.all(
          op.sourceField.map(f => {
            if (
              ['children', 'parents', 'ancestors', 'descendants'].indexOf(f) !==
              -1
            ) {
              return true
            }
            return client.redis.hexists(op.id, f)
          })
        )

        const idx = exists.findIndex(x => !!x)
        if (idx === -1) {
          return
        }

        sourceField = op.sourceField[idx]
      }

      // todo IN KEYS

      if (op.inKeys) {
        console.log('MAKE INKEYS')
      }

      const args = op.filter ? ast2rpn(op.filter, lang) : ['#1']
      console.log('SORT', op.options?.sort, args, op.filter)

      // schmall opt dont onclude id in get options if its there

      let ids = await client.redis.selva_hierarchy_find(
        '___selva_hierarchy',
        'bfs',
        sourceField,
        'order',
        op.options?.sort?.$field || '',
        op.options?.sort?.$order || 'asc',
        'offset',
        op.options.offset,
        'limit',
        op.options.limit,
        op.id,
        ...args
      )

      if (op.nested) {
        const makeNestedNice = async (ids: string[], nested: any) => {
          const makeOp = Object.assign({}, nested, {
            id: ids.map(id => id.padEnd(10, '\0')).join('')
          })

          let nids = await client.redis.selva_hierarchy_find(
            '___selva_hierarchy',
            'bfs',
            makeOp.sourceField,
            'order',
            makeOp.options?.sort?.$field || '',
            makeOp.options?.sort?.$order || 'asc',
            'offset',
            0,
            'limit',
            -1,
            makeOp.id,
            ...(makeOp.filter ? ast2rpn(makeOp.filter, lang) : ['#1'])
          )
          return nids
        }

        let isNest = op.nested
        let level = 0
        let prevIds = ids
        while (isNest) {
          let nids = []
          // console.log('LEVEL ', level, prevIds.length, prevIds)

          nids = await makeNestedNice(prevIds, isNest)
          prevIds = nids

          level++
          isNest = isNest.nested
        }

        const results = await Promise.all(
          prevIds.map(id => {
            const realOpts: any = {}
            for (const key in op.props) {
              if (!key.startsWith('$')) {
                realOpts[key] = op.props[key]
              }
            }
            return executeNestedGetOperations(
              client,
              {
                $id: id,
                ...realOpts
              },
              lang,
              db
            )
          })
        )

        if (op.single) {
          return results[0]
        }

        return results
      }

      const results = await Promise.all(
        ids.map(async id => {
          const realOpts: any = {}
          for (const key in op.props) {
            if (!key.startsWith('$')) {
              realOpts[key] = op.props[key]
            }
          }

          const x = await executeNestedGetOperations(
            client,
            {
              $id: id,
              ...realOpts
            },
            lang,
            db
          )

          return x
        })
      )

      if (op.single) {
        return results[0]
      }

      return results
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

      const nested: GetOperation[] = await Promise.all(
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
  }

  const results = await Promise.all(ops.map(myBlurx))

  const o: GetResult = {}
  results.map((r, i) => {
    if (
      ops[i].field === '' &&
      // @ts-ignore
      ops[i].single === true
    ) {
      Object.assign(o, r)

      //   return r
    } else if (r !== null && r !== undefined) {
      setNestedResult(o, ops[i].field, r)
    }
  })

  return o
}
