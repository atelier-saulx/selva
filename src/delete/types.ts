export type DeleteOptions =
  | string
  | {
      $id: string
      $hierarchy?: boolean
    }
