export type DeleteOptions =
  | string
  | {
      $db?: string
      $id: string
    }
