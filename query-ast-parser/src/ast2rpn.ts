import { Fork, FilterAST as Filter, Rpn } from './types'
import { convertNowFilter } from './convertNow'

function isFork(x: any): x is Fork {
  return x.isFork
}

function getValueType(f: Filter) {
  if (f.$operator == '<' || f.$operator == '>') {
    return 'number'
  }
  return typeof f.$value
}

const opMapString = {
  '=': 'c',
  '!=': 'c L',
  exists: 'h',
  notExists: 'h',
  has: 'a',
  includes: 'm',
}

const opMapNumber = {
  '=': 'F',
  '>': 'I',
  '<': 'H',
  '..': 'i',
  '!=': 'G',
  exists: 'h',
  notExists: 'h',
  has: 'a',
}

export default function ast2rpn(
  types: Record<string, { prefix?: string }>,
  f: Fork,
  language?: string
): Rpn {
  let out = ''
  let reg: string[] = []

  let regIndex = 1
  const regMap = {}
  const fieldNameToReg = (fieldName: string) => {
    if (regMap[fieldName]) {
      return regMap[fieldName]
    } else {
      const i = regIndex++
      reg[i] = fieldName
      regMap[fieldName] = i
      return i
    }
  }

  function ast2rpnFilter(
    types: Record<string, { prefix?: string }>,
    f: Filter,
    ignoreLang: boolean = false
  ) {
    // convert type filters to id prefix filters
    if (
      f.$operator === '=' &&
      f.$field === 'type' &&
      typeof f.$value === 'string' &&
      types[f.$value] &&
      types[f.$value].prefix
    ) {
      const prefix = types[f.$value].prefix

      const valueId = regIndex
      reg[regIndex++] = prefix

      out += ` $${valueId} e`
      if (f.isNecessary) {
        out += ' P'
      }

      return
    }

    if (f.$operator === 'textSearch') {
      out += ' #1'
      return
    }

    if (f.$field === 'ancestors') {
      out += ' #1'
      return
    }

    if (f.$operator == 'exists') {
      const fieldId = fieldNameToReg(f.$field)

      out += ` $${fieldId} h`
      return
    } else if (f.$operator == 'notExists') {
      const fieldId = fieldNameToReg(f.$field)

      out += ` $${fieldId} h L`
      return
    }

    const vType = getValueType(f)
    if (!ignoreLang && vType === 'string' && language) {
      ast2rpnFork(
        types,
        {
          isFork: true,
          $or: [
            {
              $operator: f.$operator,
              $field: f.$field,
              $value: f.$value,
            },
            {
              $operator: f.$operator,
              $field: f.$field + '.' + language,
              $value: f.$value,
            },
          ],
        },
        true
      )
      return
    } else if (vType == 'string' || vType == 'number') {
      const fieldId = fieldNameToReg(f.$field)

      let valueId: string | number = regIndex
      let isNowValue: boolean = false

      if (
        typeof f.$value === 'string' &&
        f.$value.startsWith('now') &&
        (f.$operator === '<' || f.$operator === '>')
      ) {
        valueId = convertNowFilter(<string>f.$value)
        isNowValue = true
      } else {
        reg[regIndex++] = `${f.$value}`
      }

      const op =
        vType == 'string' ? opMapString[f.$operator] : opMapNumber[f.$operator]
      if (!op) {
        console.error(`Invalid op for ${vType} field`, f)
      }

      if (isNowValue) {
        out += ` ${valueId} $${fieldId} g ${op}`
      } else if (vType === 'number') {
        out += ` @${valueId} $${fieldId} g ${op}`
      } else if (vType == 'string' && f.$operator == 'has') {
        out += ` $${valueId} $${fieldId} ${op}`
      } else { // Assume we can read the field as a string.
        out += ` $${valueId} $${fieldId} f ${op}`
      }
    } else if (vType == 'boolean') {
      const fieldId = fieldNameToReg(f.$field)
      const valueId = regIndex
      reg[regIndex++] = f.$value ? '1' : '0'

      const op = opMapNumber[f.$operator]
      if (!op) {
        console.error('Invalid op for boolean field', f)
        // TODO error
      }

      out += ` @${valueId} $${fieldId} g ${op}`
    } else if (f.$operator == '..') {
      const fieldId = fieldNameToReg(f.$field)
      let valueId1: string | number = regIndex
      let isNowValue1: boolean = false

      if (typeof f.$value[0] === 'string' && f.$value[0].startsWith('now')) {
        valueId1 = convertNowFilter(<string>f.$value[0])
        isNowValue1 = true
      } else {
        reg[regIndex++] = `${f.$value[0]}`
      }

      let valueId2: string | number = regIndex
      let isNowValue2: boolean = false
      if (typeof f.$value[1] === 'string' && f.$value[1].startsWith('now')) {
        valueId2 = convertNowFilter(<string>f.$value[1])
        isNowValue2 = true
      } else {
        reg[regIndex++] = `${f.$value[1]}`
      }

      const part1 = isNowValue1 ? valueId1 : `@${valueId1}`
      const part2 = isNowValue2 ? valueId2 : `@${valueId2}`
      out += ` ${part2} $${fieldId} g ${part1} i`
    } else if (vType === 'object') {
      const fork: Fork = {
        isFork: true,
      }
      if (f.$operator === '!=') {
        fork.$and = []

        const arr = <(string | number)[]>f.$value
        for (let i = 0; i < arr.length; i++) {
          fork.$and[i] = {
            $field: f.$field,
            $operator: f.$operator,
            $value: arr[i],
          }
        }
      } else {
        fork.$or = []

        const arr = <(string | number)[]>f.$value
        for (let i = 0; i < arr.length; i++) {
          fork.$or[i] = {
            $field: f.$field,
            $operator: f.$operator,
            $value: arr[i],
          }
        }
      }

      ast2rpnFork(types, fork)
    } else {
      // TODO error
      console.error('Type error', vType, f)
    }
  }

  function ast2rpnFork(
    types: Record<string, { prefix?: string }>,
    expr: Fork,
    ignoreLang: boolean = false
  ) {
    const lop: ' M' | ' N' = expr.$and ? ' M' : ' N'
    const arr = expr.$and || expr.$or || []

    if (arr.length === 0) {
      out += ' #1'
    } else {
      for (let i = 0; i < arr.length; i++) {
        const el = arr[i]

        if (isFork(el)) {
          ast2rpnFork(types, el)
        } else {
          ast2rpnFilter(types, el, ignoreLang)
        }

        if (i > 0) {
          out += lop
        }
      }

      if (expr.isNecessary) {
        out += ' P'
      }
    }
  }

  ast2rpnFork(types, f)

  const res = [out]
  for (let i = 1; i < regIndex; i++) {
    res[i] = reg[i]
  }
  return res
}
