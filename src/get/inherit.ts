import { Id, Language, Type, getTypeFromId } from '../schema'
import { SelvaClient } from '..'
import { GetResult, GetItem, getInner } from './'
import { getNestedField, setNestedResult } from './nestedFields'
import isEmpty from './isEmpty'
import getField from './getField'

type Ancestor = [Ancestor[], number]
// memoize this in lua (within one batch of gets)
// const ancestorMap = {} etc

const createAncestorsInner = async (
  client: SelvaClient,
  id: Id,
  s: Record<Id, Ancestor>
): Promise<Ancestor> => {
  // if memoized[id] -> get it
  if (s[id]) {
    return s[id]
  }
  const parents = await client.redis.smembers(id + '.parents')
  const ancestor: Ancestor = [[], 0]
  if (parents.length) {
    ancestor[1] = 1
    let pd = 0
    for (let pId of parents) {
      const a = await createAncestorsInner(client, pId, s)
      if (a[1] > pd) {
        pd = a[1]
        a[0].unshift(a)
      } else {
        a[0].push(a)
      }
    }
    ancestor[1] += pd
  }
  s[id] = ancestor
  return ancestor
}

const createAncestors = async (
  client: SelvaClient,
  targetId: Id
): Promise<Id[]> => {
  const s = {}
  await createAncestorsInner(client, targetId, s)
  const result = []
  // binary insert
  for (let id in s) {
    if (targetId !== id) {
      const depth = s[id][1]
      let l = 0,
        r = result.length - 1,
        m = 0
      while (l <= r) {
        m = ((l + r) / 2) | 0
        const loopDepth = s[result[m]][1]
        if (loopDepth < depth) {
          r = m - 1
          continue
        }
        l = m + 1
        if (loopDepth === depth) {
          break
        }
      }
      result.splice(l, 0, id)
    }
  }
  return result
}

const createAncestorsTypes = async (
  client: SelvaClient,
  targetId: Id,
  types: Type[]
): Promise<Id[]> => {
  const s = {}

  // memoize types and indexes
  // dont .includes
  await createAncestorsInner(client, targetId, s)
  const result = []
  // binary insert
  for (let id in s) {
    if (targetId !== id) {
      const type = getTypeFromId(id)
      const index = types.indexOf(type)
      // ugly includes can all be optmized
      if (types.includes(type)) {
        const depth = s[id][1]
        let l = 0,
          r = result.length - 1,
          m = 0
        while (l <= r) {
          m = ((l + r) / 2) | 0
          // memoize the types
          const prevType = getTypeFromId(result[m])
          if (type === prevType) {
            const loopDepth = s[result[m]][1]
            if (loopDepth < depth) {
              r = m - 1
              continue
            }
            l = m + 1
            if (loopDepth === depth) {
              break
            }
          } else {
            const loopIndex = types.indexOf(prevType)
            if (loopIndex > index) {
              r = m - 1
              continue
            }
            l = m + 1
            if (loopIndex === index) {
              break
            }
          }
        }
        result.splice(l, 0, id)
      }
    }
  }
  return result
}

const setFromAncestors = async (
  client: SelvaClient,
  ancestors: Id[],
  field: string,
  result: GetResult,
  language?: Language,
  version?: string
) => {
  for (let i = 0, len = ancestors.length; i < len; i++) {
    await getField(client, ancestors[i], field, result, language, version)
    const value = getNestedField(result, field)
    if (!isEmpty(value)) {
      break
    }
  }
}

const inherit = async (
  client: SelvaClient,
  id: Id,
  field: string,
  props: GetItem,
  result: GetResult,
  language?: Language,
  version?: string
) => {
  const inherit = props.$inherit
  if (inherit) {
    const value = getNestedField(result, field)
    if (inherit === true) {
      if (isEmpty(value)) {
        await setFromAncestors(
          client,
          await createAncestors(client, id),
          field,
          result,
          language,
          version
        )
      }
    } else if (inherit.type || inherit.name) {
      if (isEmpty(value)) {
        if (inherit.name) {
          // is name important
          console.log('inherit.name NOT IMPLEMENTED YET')
        } else {
          if (!Array.isArray(inherit.type)) {
            inherit.type = [inherit.type]
          }
          await setFromAncestors(
            client,
            await createAncestorsTypes(client, id, inherit.type),
            field,
            result,
            language,
            version
          )
        }
      }
    } else if (inherit.$item) {
      // needs to order and select (same as type order)
      if (!Array.isArray(inherit.$item)) {
        inherit.$item = [inherit.$item]
      }
      const a = await createAncestorsTypes(client, id, inherit.$item)
      const len = a.length
      if (len === 0) {
        setNestedResult(result, field, {})
      } else {
        for (let i = 0; i < len; i++) {
          const intermediateResult = {}
          await getInner(
            client,
            props,
            intermediateResult,
            a[i],
            '',
            language,
            version,
            true
          )
          // want not isEmpty but has results or something like that
          let empty = false
          for (let key in props) {
            if (key[0] !== '$') {
              // needs to be much better getinner needs to return if all requirements are met...
              if (isEmpty(intermediateResult[key])) {
                empty = true
                break
              }
            }
          }
          if (!empty) {
            setNestedResult(result, field, intermediateResult)
            break
          }
          if (i === len - 1) {
            setNestedResult(result, field, {})
          }
        }
      }
    }
  }
}

export default inherit
