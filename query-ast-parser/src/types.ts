import { Schema, SearchSchema } from '../../client/src/schema/types'

export type Value = (string | number) | (string | number)[]

export type FilterAST = {
  $field: string
  $operator:
    | '='
    | '>'
    | '<'
    | '..'
    | '!='
    | 'distance'
    | 'exists'
    | 'notExists'
    | 'textSearch'
  $value?: Value
  // $search: string[]
  hasNow?: true
}

export type Fork = {
  $and?: (Fork | FilterAST)[]
  $or?: (Fork | FilterAST)[]
  ids?: string[]
  isFork: true
}

export type FieldSubscription = {
  $value: (string | number)[]
  $operator: '=' | '>' | '<' | '..' | '!='
}

export type WithRequired<T, K extends keyof T> = Omit<T, K> &
  Required<Pick<T, K>>
