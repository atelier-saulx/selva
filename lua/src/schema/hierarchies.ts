import { Schema, Types } from '~selva/schema/index'
import { isEqual } from '../util'

function updateHierarchiesForType(_typeName: string): void {
  // TODO
}

export default function updateHierarchies(
  // fields are required
  oldSchema: Schema,
  newSchema: Schema
): void {
  for (const typeName in newSchema.types) {
    if (
      oldSchema.types[typeName] &&
      !isEqual(oldSchema[typeName].hierarchy, newSchema[typeName].hierarchy)
    ) {
      updateHierarchiesForType(typeName)
    }
  }
}
