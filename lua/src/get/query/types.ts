import { GetOptions, Sort } from '~selva/get/types'

export type Value = (string | number) | (string | number)[]

export type FilterAST = {
  $field: string
  $operator: '=' | '>' | '<' | '..' | '!=' | 'distance'
  $value: Value
  $search: string[]
  hasNow?: true
}

export type Fork = {
  $and?: (Fork | FilterAST)[]
  $or?: (Fork | FilterAST)[]
  isFork: true
}

export type Meta = {
  ast?: Fork | undefined
  sort?: Sort | Sort[]
  traverse?: string | string[]
  ids: string[]
  type?: string[]
}

export type FieldSubscription = {
  $value: (string | number)[]
  $operator: '=' | '>' | '<' | '..' | '!='
}

export type QuerySubscription = {
  idFields?: Record<string, true>
  queryId: string
  ids?: Record<string, true>
  member: { $field: string; $value: string[] }[] // array is an OR
  type?: string[]
  fields: {
    [key: string]: true
  }
  time?: { nextRefresh: number }
}
