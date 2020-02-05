import { Filter } from '~selva/get/types'

export type Value = (string | number) | (string | number)[]

export type FilterAST = {
  $field: string
  $operator: '=' | '>' | '<' | '..' | '!=' | '<=' | '>='
  $value: Value
  $search: string[]
}

export type Fork = {
  $and?: (Fork | FilterAST)[]
  $or?: (Fork | FilterAST)[]
  isFork: true
}
