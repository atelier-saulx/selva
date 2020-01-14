// only types can be imported with `paths`, not supported by tstl
import { Id, Type, typePrefix } from '../../src/schema'

export default function getTypeFromId(id: Id): Type {
  if (id === 'root') {
    return 'root'
  }

  return typePrefix[id.substring(0, 2)]
}
