export type DeleteOptions =
  | string
  | ({
      $db?: string
      $id: string
      $hierarchy?: boolean
    } & Record<string, any>)
