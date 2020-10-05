import { Fork, FilterAST, Value, WithRequired } from './types'
import isFork from './isFork'
import convertNow from './convertNow'

const valueIsEqual = (a: Value, b: Value, strict: boolean): boolean => {
  if (a === b) {
    return true
  }

  if (Array.isArray(a) && Array.isArray(b)) {
    if (a.length !== b.length) {
      return false
    }
    for (let i = 0; i < a.length; i++) {
      if (strict ? b[i] !== a[i] : b.indexOf(a[i]) == -1) {
        return false
      }
    }
    return true
  }
  return false
}

const filterIsEqual = (a: FilterAST, b: FilterAST): boolean => {
  return (
    a.$operator === b.$operator &&
    a.$field === b.$field &&
    valueIsEqual(a.$value, b.$value, a.$operator === '..')
  )
}

const isEqual = (a: FilterAST, b: FilterAST): [boolean, null | string] => {
  if (Array.isArray(b.$value)) {
    if (Array.isArray(a.$value)) {
      console.log('a is array need to loop to reduce', a.$value, b.$value)
    } else if (b.$value.indexOf(a.$value) !== -1) {
      a.$value = b.$value
      return [false, null]
    }
  }
  // if a field is a tag then you can have multiple values
  if (a.$search.indexOf('TAG') === -1 && a.$value !== b.$value) {
    return [
      false,
      `Cannot have 2 isEqual conditions @${a.$field} (${a.$value}) and (${b.$value})`
    ]
  }
  return [true, null]
}

const isNotEqual = (a: FilterAST, b: FilterAST): [boolean, null | string] => {
  if (a.$value !== b.$value) {
    if (!Array.isArray(b.$value)) {
      if (!Array.isArray(a.$value)) {
        a.$value = [a.$value, b.$value]
      } else if (a.$value.indexOf(b.$value) === -1) {
        a.$value[a.$value.length] = b.$value
      }
    } else {
      if (!Array.isArray(a.$value)) {
        if (b.$value.indexOf(a.$value) !== -1) {
          a.$value = b.$value
        } else {
          b.$value[b.$value.length] = a.$value
          a.$value = b.$value
        }
      } else {
        for (let i = 0; i < b.$value.length; i++) {
          if (a.$value.indexOf(b.$value[i]) === -1) {
            a.$value[a.$value.length] = b.$value[i]
          }
        }
      }
    }
  }
  return [false, null]
}

const isNotEqualAndIsEqual = (
  a: FilterAST,
  b: FilterAST
): [boolean, null | string] => {
  // needs more
  if (a.$value === b.$value) {
    return [
      false,
      `Cannot have something equal and inequal @${a.$field} (${a.$operator}${a.$value}) and (${b.$operator}${b.$value})`
    ]
  }
  return [false, null]
}

const isRangeAndLargerOrSmaller = (
  a: FilterAST,
  b: FilterAST
): [boolean, null | string] => {
  let range = a
  let other = b
  if (b.$operator === '..') {
    range = b
    other = a
  }

  const otherVal =
    typeof other.$value === 'string' && other.$value.startsWith('now')
      ? convertNow(other.$value)
      : other.$value

  if (other.$operator === '>') {
    const rangeVal =
      typeof range.$value[1] === 'string' && range.$value[1].startsWith('now')
        ? convertNow(range.$value[1])
        : range.$value[1]

    if (otherVal > rangeVal) {
      return [
        false,
        `Out of bounds range filter ${other.$value} < ${range.$value}`
      ]
    }
  }
  if (other.$operator === '<') {
    const rangeVal =
      typeof range.$value[0] === 'string' && range.$value[0].startsWith('now')
        ? convertNow(range.$value[0])
        : range.$value[0]

    if (otherVal > rangeVal) {
      return [
        false,
        `Out of bounds range filter ${other.$value} > ${range.$value}`
      ]
    }
  }
  if (b.$operator === '..') {
    a.$operator = '..'
    a.$value = b.$value
  }
  return [false, null]
}

const isLargerThenAndSmallerThen = (
  a: FilterAST,
  b: FilterAST
): [boolean, null | string] => {
  let $lo: FilterAST, $hi: FilterAST
  if (a.$operator === '>') {
    $lo = a
    $hi = b
  } else {
    $lo = b
    $hi = a
  }

  if (Array.isArray($lo.$value) || Array.isArray($hi.$value)) {
    return [false, 'Dont use arrays in larger then or smaller then']
  }

  a.$value = [$lo.$value, $hi.$value]
  a.$operator = '..'
  return [false, null]
}

const isLargerThenAndLargerThen = (
  a: FilterAST,
  b: FilterAST
): [boolean, null | string] => {
  if (
    (typeof a.$value === 'string' && a.$value.startsWith('now')) ||
    (typeof b.$value === 'string' && b.$value.startsWith('now'))
  ) {
    return [false, null]
  }

  if (b.$value > a.$value) {
    a.$value = b.$value
  }
  return [false, null]
}

const isSmallerThenAndSmallerThen = (
  a: FilterAST,
  b: FilterAST
): [boolean, null | string] => {
  if (
    (typeof a.$value === 'string' && a.$value.startsWith('now')) ||
    (typeof b.$value === 'string' && b.$value.startsWith('now'))
  ) {
    return [false, null]
  }

  if (b.$value < a.$value) {
    a.$value = b.$value
  }
  return [false, null]
}

function reduceAnd(fork: WithRequired<Fork, '$and'>): string | null {
  const reduced: Record<string, FilterAST[]> = {}
  const forks: Fork[] = []
  for (let i = 0; i < fork.$and.length; i++) {
    const filter = fork.$and[i]
    if (!isFork(filter)) {
      if (!reduced[filter.$field]) {
        reduced[filter.$field] = []
      }
      const filters = reduced[filter.$field]
      let addFilter = true
      for (let j = 0; j < filters.length; j++) {
        const prevFilter = filters[j]
        if (filterIsEqual(prevFilter, filter)) {
          addFilter = false
          break
        } else {
          const $a = prevFilter.$operator
          const $b = filter.$operator
          let fn: undefined | ((a: any, b: any) => [boolean, string | null])
          if ($a === '=' && $b === '=') {
            fn = isEqual
          } else if ($a === '!=' && $b === '!=') {
            fn = isNotEqual
          } else if (
            ($a === '=' && $b === '!=') ||
            ($a === '!=' && $b === '=')
          ) {
            fn = isNotEqualAndIsEqual
          } else if (($a === '>' && $b === '<') || ($a === '<' && $b === '>')) {
            fn = isLargerThenAndSmallerThen
          } else if ($a === '>' && $b === '>') {
            fn = isLargerThenAndLargerThen
          } else if ($a === '<' && $b === '<') {
            fn = isSmallerThenAndSmallerThen
          } else if (
            // auto merge smaller then and equal then arrays or just dont allow them
            ($a === '..' && ($b === '>' || $b === '<')) ||
            ($b === '..' && ($a === '>' || $a === '<'))
          ) {
            fn = isRangeAndLargerOrSmaller
          }

          if (fn) {
            const [add, err] = fn(prevFilter, filter)
            if (err) {
              return err
            }
            if (!add) {
              addFilter = false
              break
            }
          }
        }
      }
      if (addFilter) {
        filters[filters.length] = filter
      }
    } else {
      forks[forks.length] = filter
    }
  }

  const $and: (FilterAST | Fork)[] = []

  for (let field in reduced) {
    const filters = reduced[field]
    for (let i = 0; i < filters.length; i++) {
      $and[$and.length] = filters[i]
    }
  }

  for (let j = 0; j < forks.length; j++) {
    $and[$and.length] = forks[j]
  }

  fork.$and = $and
  return null
}

export default reduceAnd
