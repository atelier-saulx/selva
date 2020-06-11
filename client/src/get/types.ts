import { Id } from '../schema/index'

export type Inherit =
  | boolean
  | {
      $type?: string | string[]
      $name?: string | string[]
      $item?: Id | Id[]
      $merge: boolean
      $required?: Id | Id[]
    }

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
  $value: undefined // makes copmiling this easier, nice...
  $and?: Filter
  $or?: Filter
}

export type Filter =
  | ExistsFilter
  | GeoFilter
  | {
      $operator: '=' | '!=' | '>' | '<' | '..'
      $field: string
      $value: string | number | (string | number)[]
      $and?: Filter
      $or?: Filter
    }

export type TraverseOptions = {
  $db?: string
  $id?: string
  $field: string
  // TODO: add $filter, $limit, $offset
}

export type Find = {
  $db?: string
  $traverse?: 'descendants' | 'ancestors' | string | string[] | TraverseOptions
  $filter?: Filter | Filter[]
  $find?: Find
}

export type Sort = {
  $field: string
  $order?: 'asc' | 'desc'
}

export type List =
  | true
  | {
      $offset?: number
      $limit?: number
      $sort?: Sort | Sort[]
      $find?: Find
      $inherit?: Inherit
    }

export type GetField<T> = {
  $field?: string | string[] | { path: string | string[]; value: GetOptions }
  $inherit?: Inherit
  $list?: List
  $find?: Find
  $default?: T
  $all?: boolean
  $value?: any
}

// want with $ come on :D
export type Item = {
  [key: string]: any
}

// update $language for default + text (algebraic)
export type GetItem<T = Item> = {
  [P in keyof T]?: T[P] extends Item[]
    ? GetItem<T>[] | true
    : T[P] extends object
    ? (GetItem<T[P]> & GetField<T>) | true
    : T[P] extends number
    ? T[P] | GetField<T[P]> | true
    : T[P] extends string
    ? T[P] | GetField<T[P]> | true
    : T[P] extends boolean
    ? T[P] | GetField<T[P]>
    : (T[P] & GetField<T[P]>) | true
} &
  GetField<T> & {
    [key: string]: any
  }

export type GetResult = {
  [key: string]: any
}

export type GetOptions = GetItem & {
  $id?: Id | Id[]
  $alias?: string | string[]
  $version?: string
  $language?: string
  $rawAncestors?: true
}
