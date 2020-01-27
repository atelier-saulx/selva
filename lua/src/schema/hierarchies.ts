import { Schema } from '~selva/schema/index'
import { isEqual } from '../util'

function updateHierarchiesForType(typeName: string): void {
  // TODO
}

export default function updateHierarchies(
  oldSchema: Schema,
  newSchema: Schema
): void {
  for (const typeName in newSchema.types) {
    if (
      oldSchema.types[typeName] &&
      !isEqual(oldSchema.hierarchy, newSchema.hierarchy)
    ) {
      updateHierarchiesForType(typeName)
    }
  }
}
