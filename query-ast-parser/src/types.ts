export type Value = (string | number) | (string | number)[]

export type GeoFilter = {
  $operator: 'distance'
  $field: string
  $value: {
    $lat: number
    $lon: number
    $radius: number
  }
  $and?: Filter
  $or?: Filter
}

export type ExistsFilter = {
  $operator: 'exists' | 'notExists'
  $field: string
  $value?: undefined // makes compiling this easier, nice...
  $and?: Filter
  $or?: Filter
}

export type Filter =
  | ExistsFilter
  | GeoFilter
  | {
      $operator: '=' | '!=' | '>' | '<' | '..' | 'has' | 'includes' | 'textSearch'
      $field: string
      $value: string | number | (string | number)[]
      $and?: Filter
      $or?: Filter
    }

export type FilterAST = {
  $field: string
  $operator:
    | '='
    | '>'
    | '<'
    | '..'
    | '!='
    | 'has'
    | 'includes'
    | 'distance'
    | 'exists'
    | 'notExists'
    | 'textSearch'
  $value?: Value
  // $search: string[]
  hasNow?: true
  isNecessary?: true
}

export type Fork = {
  $and?: (Fork | FilterAST)[]
  $or?: (Fork | FilterAST)[]
  ids?: string[]
  isFork: true
  isNecessary?: true
}

export type TraverseByTypeExpression =
  | false
  | string
  | {
      $first?: TraverseByTypeExpression[]
      $all?: TraverseByTypeExpression[]
    }

export type TraverseByType = {
  $any: TraverseByTypeExpression
  [k: string]: TraverseByTypeExpression
}
export type FieldSubscription = {
  $value: (string | number)[]
  $operator: '=' | '>' | '<' | '..' | 'has' | '!='
}

export type WithRequired<T, K extends keyof T> = Omit<T, K> &
  Required<Pick<T, K>>

export type Rpn = string[]
