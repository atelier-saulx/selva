import { Item, Id, Language, Type, Text, Field, languages } from '../schema'
import { SelvaClient } from '..'
import { Inherit, GetResult, GetOptions, GetItem } from './'
import { getNestedField, setNestedResult } from './nestedFields'
import isEmpty from './isEmpty'

const inherit = async (
  client: SelvaClient,
  id: Id,
  field: string,
  inherit: true | Inherit,
  props: GetItem,
  result: GetResult
) => {
  console.log('snurkels', inherit, props)
  if (inherit === true) {
    console.log('go for it')
    const value = getNestedField(result, field)
    if (isEmpty(value)) {
      // setNestedResult(result, field, props.$default)
    }
  }
}

export default inherit
