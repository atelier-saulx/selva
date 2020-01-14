import { Item, Id, Language, Type, Text, Field, languages } from '../schema'
import { SelvaClient } from '..'
import { Inherit, GetResult, GetOptions, GetItem, getInner } from './'
import { getNestedField, setNestedResult } from './nestedFields'
import isEmpty from './isEmpty'

type Ancestor = [Ancestor[], number]
// memoize this in lua
// const ancestorMap = {} etc

const createAncestorsInner = async (
  client: SelvaClient,
  id: Id,
  s: Record<Id, Ancestor>
): Promise<Ancestor> => {
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

const createAncestors = async (client: SelvaClient, id: Id): Promise<Id[]> => {
  const s = {}
  await createAncestorsInner(client, id, s)
  const result = []
  let largest = 0
  // order depends on inherit type
  for (let id in s) {
    const depth = s[id][1]
    let l = 0,
      r = result.length - 1,
      m: number
    while (l <= r) {
      m = ((l + r) / 2) | 0
      const loopDepth = s[result[m]][1]
      // or if type is more prefered
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
  return result
}

const inherit = async (
  client: SelvaClient,
  id: Id,
  field: string,
  inherit: true | Inherit,
  props: GetItem,
  result: GetResult,
  language?: Language
) => {
  const value = getNestedField(result, field)
  if (inherit === true && isEmpty(value)) {
    const a = await createAncestors(client, id)
    console.log('ANCESTORS', a)
  }
}

export default inherit
