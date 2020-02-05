import { Filter } from '~selva/get/types'

export type FilterAST = {
  $field: string
  $operator: '=' | '>' | '<' | '..' | '!=' | '<=' | '>='
  $value: (string | number) | (string | number)[]
  $search: string[]
}

export type Fork = {
  $and?: (Fork | FilterAST)[]
  $or?: (Fork | FilterAST)[]
  isFork: true
}

export type QeuryResult = {
  filters: Fork
  reverseMap: Record<string, FilterAST[]>
}
