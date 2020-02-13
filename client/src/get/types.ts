import { Id } from '../schema/index'

export type Inherit =
  | boolean
  | {
      $type?: string | string[]
      $name?: string | string[]
      $item?: Id | Id[]
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

export type Filter =
  | GeoFilter
  | {
      $operator: '=' | '!=' | '>' | '<' | '..'
      $field: string
      $value: string | number | (string | number)[]
      $and?: Filter
      $or?: Filter
    }

export type Find = {
  $traverse?: 'descendants' | 'ancestors' | string | string[]
  $filter?: Filter | Filter[]
  $find?: Find
}

export type Sort = {
  $field: string
  $order?: 'asc' | 'desc'
}

export type List = {
  $range?: [number, number]
  $sort?: Sort | Sort[]
  $find?: Find
}

export type GetField<T> = {
  $field?: string | string[]
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
  $id?: Id
  $version?: string
  $language?: string
}
