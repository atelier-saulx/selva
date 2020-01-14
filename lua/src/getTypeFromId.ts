import { Id, Type, typePrefix } from 'selva/schema'

export default function getTypeFromId(id: Id): Type {
  if (id === 'root') {
    return 'root'
  }

  return typePrefix[id.substring(0, 2)]
}
