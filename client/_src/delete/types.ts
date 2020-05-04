export type DeleteOptions =
  | string
  | ({
      $id: string
      $hierarchy?: boolean
    } & Record<string, any>)
