import globals from './globals'

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

export function joinAny(strs: any[], delim: string): string {
  let str = ''
  for (let i = 0; i < strs.length - 1; i++) {
    str += tostring(strs[i]) + delim
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

export function stringEndsWith(str: string, slice: string): boolean {
  if (slice.length > str.length) {
    return false
  }

  for (let i = 0; i < slice.length; i++) {
    if (str[str.length - 1 - i] !== slice[slice.length - 1 - i]) {
      return false
    }
  }

  return true
}

export function ensureArray<T>(value: (T | T[] | null | undefined) | T[]): T[] {
  if (isArray(value)) {
    return <T[]>value
  }

  if (value === null) {
    return <T[]>[]
  }

  return [<T>value]
}

export function emptyArray(): never[] {
  globals.NEEDS_GSUB = true
  // @ts-ignore
  return ['___selva_empty_array']
}

export function markEmptyArraysInJSON(str: string): string {
  const [marked, replaceCount] = string.gsub(
    str,
    '(%s*:%s*)%[%]',
    '%1["___selva_empty_array"]'
  )

  if (replaceCount > 0) {
    globals.NEEDS_GSUB = true
    return marked
  }

  return str
}

export const isEqual = (a: any, b: any): boolean => {
  const typeA = type(a)
  if (typeA !== type(b)) {
    return false
  }

  if (typeA === 'table') {
    for (let key in a) {
      if (!b[key]) {
        return false
      } else {
        if (!isEqual(a[key], b[key])) {
          return false
        }
      }
    }
  } else if (a !== b) {
    return false
  }
  return true
}

export function testString(str: string, regex: string): boolean {
  return string.find(str, regex) !== null
}

export function objectAssign(
  base: Record<string, any>,
  ...others: Record<string, any>[]
): Record<string, any> {
  for (const obj of others) {
    for (const key in obj) {
      base[key] = obj[key]
    }
  }

  return base
}

export function indexOf(a: any[], b: any): number {
  for (let i = 0, len = a.length; i < len; i++) {
    if (a[i] === b) {
      return i
    }
  }
  return -1
}

export function now(): number {
  const [sec, micro] = redis.call('time')
  return Math.floor(tonumber(sec) / 1000 + tonumber(micro) * 1000)
}
