import { Id } from './schema'
import { SetOptions } from './setTypes'
import { DeleteOptions } from './deleteTypes'

export type ModifyOptionsUpdate = {
  kind: 'update'
  payload: SetOptions & { $id: string }
}

export type ModifyOptionsDelete = {
  kind: 'delete'
  payload: DeleteOptions
}

export type ModifyOptions = ModifyOptionsUpdate | ModifyOptionsDelete

export type ModifyResult = Id | boolean
