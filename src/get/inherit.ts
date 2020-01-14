import { Item, Id, Language, Type, Text, Field, languages } from '../schema'
import { SelvaClient } from '..'
import { Inherit, GetResult, GetOptions, GetItem, getInner } from './'
import { getNestedField, setNestedResult } from './nestedFields'
import isEmpty from './isEmpty'

type Ancestor = {
  id: Id
  parents: Ancestor[]
  depth?: number
}

const countDepth = (ancestor: Ancestor): number => {
  let d = 0
  if (ancestor.depth !== void 0) {
    return ancestor.depth
  }
  if (ancestor.parents.length === 0) {
    d = 0
  } else {
    d = 1
    let pd = 0
    for (let a of ancestor.parents) {
      let x = countDepth(a)
      if (x > pd) {
        pd = x
      }
      d += pd
    }
  }
  ancestor.depth = d
  return d
}

const createAncestors = async (
  client: SelvaClient,
  id: Id
): Promise<Ancestor> => {
  const parents = await client.redis.smembers(id + '.parents')
  const ancestor: Ancestor = { id, parents: [], depth: 0 }
  if (parents.length) {
    ancestor.depth = 1
    let pd = 0
    for (let pId of parents) {
      const a = await createAncestors(client, pId)
      if (a.depth > pd) {
        pd = a.depth
      }
      ancestor.parents.push(a)
    }
    ancestor.depth = pd
  }
  return ancestor
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
  console.log('snurkels', inherit, props)
  const value = getNestedField(result, field)
  if (inherit === true && isEmpty(value)) {
    const a = await createAncestors(client, id)
    console.log('ANCESTORS', JSON.stringify(a, void 0, 2))
  }
}

export default inherit
