import { Id } from './schema'

export type DeleteOptions =
  | Id
  | {
      $id: Id
      $hierarchy?: boolean
    }
