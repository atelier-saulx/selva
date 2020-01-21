export type BaseItem = { [key: string]: any }

export type RedisSetParams =
  | Id[]
  | {
      $value?: string[] | Id
      $add?: Id[] | Id
      $delete?: Id[] | Id
    }

export type HierarchySet = RedisSetParams & {
  $hierarchy?: boolean
}

export type SetExtraOptions<T> = {
  $default?: T
  $value?: T
  $merge?: boolean
  $field?: string | string[]
}

export type SetExtraCounterOptions = {
  $increment?: number
}

export type SetItem<T = BaseItem> = {
  [P in keyof T]?: T[P] extends BaseItem[]
    ? SetItem<T>[]
    : T[P] extends object
    ? SetItem<T[P]> & SetExtraOptions<T>
    : T[P] extends number
    ? T[P] | (SetExtraOptions<T[P]> & SetExtraCounterOptions)
    : T[P] extends string
    ? T[P] | SetExtraOptions<T[P]>
    : T[P] extends boolean
    ? T[P] | SetExtraOptions<T[P]>
    : T[P] | (T[P] & SetExtraOptions<T[P]>)
}

export type SetOptions = SetItem & {
  $id?: Id
  $merge?: boolean
  $version?: string
  children?: HierarchySet | SetItem[]
  parents?: HierarchySet
  externalId?:
    | ExternalId[]
    | {
        $add?: ExternalId[] | ExternalId
        $delete?: ExternalId[] | ExternalId
        $value?: ExternalId[]
      }
  auth?: {
    password?: string
    google?: string
    facebook?: string
    role?: {
      id?: RedisSetParams
      type?: UserType
    }
  }
}
