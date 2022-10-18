import {
  DeleteField,
  FieldInputSchema,
  InputTypeSchema,
  isDeleteField,
} from '.'

export default (t: InputTypeSchema): string[][] => {
  const fields = []
  if (t.fields) {
    const walkFields = (
      x: FieldInputSchema | DeleteField,
      prev: any,
      arr: string[]
    ) => {
      if (isDeleteField(x)) {
        fields.push(arr)
        delete prev[arr[arr.length - 1]]
        console.log('DELETE', arr)
        return true
      }

      if ('properties' in x) {
        for (const f in x.properties) {
          walkFields(x.properties[f], x.properties, [...arr, f])
        }
      } else if ('items' in x) {
        if (walkFields(x.items, x.items, arr)) {
          // @ts-ignore
          x.items = {}
        }
      } else if ('values' in x) {
        if (walkFields(x.values, x.values, arr)) {
          // @ts-ignore
          x.values = {}
        }
      }
    }
    for (const f in t.fields) {
      walkFields(t.fields[f], t.fields, [f])
    }
  }
  return fields
}
