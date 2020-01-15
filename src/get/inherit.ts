import { Id, Language, Type, getTypeFromId } from '../schema'
import { SelvaClient } from '..'
import { GetResult, GetItem, getInner } from './'
import { setNestedResult } from './nestedFields'
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
    if (targetId === id) {
      continue
    }
    const depth = s[id][1]
    let l = 0,
      r = result.length - 1,
      m = 0
    while (l <= r) {
      m = ((l + r) / 2) | 0
      const prevDepth = s[result[m]][1]
      if (prevDepth < depth) {
        r = m - 1
        continue
      }
      l = m + 1
      if (prevDepth === depth) {
        break
      }
    }
    // replace with insert
    result.splice(l, 0, id)
  }
  return result
}

const createAncestorsFromFields = async (
  client: SelvaClient,
  targetId: Id,
  fields: string[],
  // not async in lua
  parse: (client: SelvaClient, id: Id) => Promise<string>
): Promise<Id[]> => {
  const s = {}
  await createAncestorsInner(client, targetId, s)
  const result = []
  for (let id in s) {
    if (targetId === id) {
      continue
    }
    const ancestor = s[id]
    // get type/name index , store it for faster lookup
    if (ancestor.length === 2) {
      const value = await parse(client, id)
      if (!value) {
        continue
      }
      let ignore = false
      for (let i = 0, len = fields.length; i < len; i++) {
        if (fields[i] === value) {
          ancestor.push(i, value)
          break
        } else if (i === len - 1) {
          ignore = true
        }
      }
      if (ignore) {
        continue
      }
    }
    const depth = ancestor[1]
    const index = ancestor[2]
    const value = ancestor[3]
    // binary insert
    let l = 0,
      r = result.length - 1,
      m = 0
    while (l <= r) {
      m = ((l + r) / 2) | 0
      const prev = s[result[m]]
      const prevValue = prev[3]
      if (value === prevValue) {
        const prevDepth = prev[1]
        if (prevDepth < depth) {
          r = m - 1
          continue
        }
        l = m + 1
        if (prevDepth === depth) {
          break
        }
      } else {
        const prevIndex = prev[2]
        if (prevIndex > index) {
          r = m - 1
          continue
        }
        l = m + 1
        if (prevIndex === index) {
          break
        }
      }
    }
    result.splice(l, 0, id)
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
    if (
      await getField(client, ancestors[i], field, result, language, version)
    ) {
      break
    }
  }
}

const parseName = async (client: SelvaClient, id: Id): Promise<string> => {
  return await client.redis.hget(id, 'name')
}

const parseType = async (client: SelvaClient, id: Id): Promise<string> => {
  return getTypeFromId(id)
}

const inheritItem = async (
  client: SelvaClient,
  id: Id,
  field: string,
  props: GetItem,
  result: GetResult,
  item: Type[],
  language?: Language,
  version?: string
) => {
  const ancestors = await createAncestorsFromFields(client, id, item, parseType)
  const len = ancestors.length
  if (len === 0) {
    setNestedResult(result, field, {})
  } else {
    for (let i = 0; i < len; i++) {
      const intermediateResult = {}
      const isComplete = await getInner(
        client,
        props,
        intermediateResult,
        ancestors[i],
        '',
        language,
        version,
        '$inherit'
      )
      if (isComplete || i === len - 1) {
        setNestedResult(result, field, intermediateResult)
        break
      }
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
    if (inherit === true) {
      await setFromAncestors(
        client,
        await createAncestors(client, id),
        field,
        result,
        language,
        version
      )
    } else if (inherit.$type || inherit.$name) {
      let ancestors: Id[]
      if (inherit.$name) {
        if (!Array.isArray(inherit.$name)) {
          inherit.$name = [inherit.$name]
        }
        ancestors = await createAncestorsFromFields(
          client,
          id,
          inherit.$name,
          parseName
        )
      } else {
        if (!Array.isArray(inherit.$type)) {
          inherit.$type = [inherit.$type]
        }
        ancestors = await createAncestorsFromFields(
          client,
          id,
          inherit.$type,
          parseType
        )
      }
      await setFromAncestors(
        client,
        ancestors,
        field,
        result,
        language,
        version
      )
    } else if (inherit.$item) {
      if (!Array.isArray(inherit.$item)) {
        inherit.$item = [inherit.$item]
      }
      await inheritItem(
        client,
        id,
        field,
        props,
        result,
        inherit.$item,
        language,
        version
      )
    }
  }
}

export default inherit
