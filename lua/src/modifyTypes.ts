import { SetOptions } from '../../src/setTypes'
import { DeleteOptions } from '../../src/deleteTypes'

export type ModifyOptionsUpdate = {
  kind: 'update'
  payload: SetOptions & { $id: string }
}

export type ModifyOptionsDelete = {
  kind: 'delete'
  payload: DeleteOptions
}

export type ModifyOptions = ModifyOptionsUpdate | ModifyOptionsDelete
