export type Value = (string | number) | (string | number)[]

export type FilterAST = {
  $field: string
  $operator: '=' | '>' | '<' | '..' | '!=' | 'distance' | 'exists' | 'notExists'
  $value: Value
  $search: string[]
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
