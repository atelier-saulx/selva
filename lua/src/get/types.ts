import { GetResult, GetItem } from '~selva/get/types'
import { Id, Schema } from '~selva/schema/index'

export type GetFieldFn = (
  props: GetItem,
  schemas: Schema,
  result: GetResult,
  id: Id,
  field?: string,
  language?: string,
  version?: string,
  includeMeta?: boolean,
  ignore?: '$' | '$inherit' | '$list' | '$find' | '$filter' // when from inherit
) => boolean
