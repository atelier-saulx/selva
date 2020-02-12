import { GetOptions } from '~selva/get/types'

export type Value = (string | number) | (string | number)[]

export type FilterAST = {
  $field: string
  $operator: '=' | '>' | '<' | '..' | '!='
  $value: Value
  $search: string[]
}

export type Fork = {
  $and?: (Fork | FilterAST)[]
  $or?: (Fork | FilterAST)[]
  isFork: true
}

export type Meta = {
  ast: Fork | undefined
  getOptions: GetOptions
}

export type FieldSubscription = {
  $value: (string | number)[]
  $operator: '=' | '>' | '<' | '..' | '!='
}

export type QuerySubscription = {
  member: { $field: string; $value: (string | number)[] }[]
  fields: Record<string, []>
}
