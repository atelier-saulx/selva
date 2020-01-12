import { GetResult } from './'

const setNestedResult = (result: GetResult, field: string, value: any) => {
  const fields = field.split('.')
  const len = fields.length
  if (len > 1) {
    let segment = result
    for (let i = 0; i < len - 1; i++) {
      segment = segment[fields[i]] || (segment[fields[i]] = {})
    }
    segment[fields[len - 1]] = value
  } else {
    result[field] = value
  }
  return value
}

export default setNestedResult
