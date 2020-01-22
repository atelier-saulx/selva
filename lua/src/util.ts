export const arrayIsEqual = (a: any[], b: any[]): boolean => {
  const len = a.length
  if (len !== b.length) {
    return false
  }
  for (let i = 0; i < len; i++) {
    if (a[i] !== b[i]) {
      return false
    }
  }
  return true
}

export function isString(val: any): val is string {
  return type(val) === 'string'
}

export function isArray<T>(value: T | T[]): value is T[] {
  if (type(value) === 'table') {
    if ((<T[]>value).length > 0 && value[0] !== null) {
      return true
    } else if ((<T[]>value).length === 0 && next(value) === null) {
      return true
    }
  }

  return false
}

export function splitString(str: string, delim: string): string[] {
  let strings: string[] = []
  let idx = 0
  let current = ''
  for (let i = 0; i < str.length; i++) {
    if (str[i] === delim) {
      strings[idx] = current
      idx++
      current = ''
    } else {
      current += str[i]
    }
  }
  if (current.length > 0) {
    strings[idx] = current
  }
  return strings
}

export function joinString(strs: string[], delim: string): string {
  let str = ''
  for (let i = 0; i < strs.length - 1; i++) {
    str += strs[i] + delim
  }

  str += strs[strs.length - 1] || ''
  return str
}

export function stringStartsWith(str: string, slice: string): boolean {
  if (slice.length > str.length) {
    return false
  }

  for (let i = 0; i < slice.length; i++) {
    if (str[i] !== slice[i]) {
      return false
    }
  }

  return true
}
