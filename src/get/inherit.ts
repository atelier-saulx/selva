import { Item, Id, Language, Type, Text, Field, languages } from '../schema'
import { SelvaClient } from '..'
import { Inherit, GetResult, GetOptions, GetItem, getInner } from './'
import { getNestedField, setNestedResult } from './nestedFields'
import isEmpty from './isEmpty'

const lookUp = async (client: SelvaClient, field: string) => {}

const inheritTrue = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language,
  passed: Record<Id, true> = {}
) => {
  const parents = await client.redis.smembers(id + '.parents')
  console.log(parents)
  const props = {
    $inherit: true
  }

  // zip up
  for (let pId of parents) {
    await getInner(client, props, result, pId, field, language)
    const value = getNestedField(result, field)
    if (!isEmpty(value)) {
      return
    }
  }

  // passed
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
    await inheritTrue(client, id, field, result, language)
  }
}

export default inherit
