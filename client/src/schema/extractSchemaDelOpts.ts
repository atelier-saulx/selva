import { DeleteField, FieldSchema, InputTypeSchema, isDeleteField } from '.'

export default (t: InputTypeSchema): string[][] => {
  const fields = []
  if (t.fields) {
    const walkFields = (
      x: FieldSchema | DeleteField,
      prev: any,
      arr: string[]
    ) => {
      if (isDeleteField(x)) {
        fields.push(arr)
        delete prev[arr.length - 1]
        return
      }
      if (x.type === 'object') {
        for (const f in x.properties) {
          walkFields(x.properties[f], x.properties, [...arr, f])
        }
        return
      }
      if (x.type === 'array') {
        for (const f in x.items) {
          walkFields(x.items[f], x.items, [...arr, f])
        }
      }
    }
    for (const f in t.fields) {
      walkFields(t.fields[f], t.fields, [f])
    }
  }
  return fields
}
