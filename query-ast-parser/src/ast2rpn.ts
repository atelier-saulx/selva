import { Fork, FilterAST as Filter } from './types'
import convertNow from './convertNow'

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
  notExists: 'h'
}

const opMapNumber = {
  '=': 'F',
  '>': 'I',
  '<': 'H',
  '..': 'i',
  '!=': 'G',
  exists: 'h',
  notExists: 'h'
}

export default function ast2rpn(
  f: Fork,
  language?: string
): [string[] | undefined, string[]] {
  let findIn: string[] | undefined = undefined
  let out = ''
  let reg: string[] = []
  let regIndex = 1

  function ast2rpnFilter(f: Filter, ignoreLang: boolean = false) {
    if (f.$operator === 'textSearch') {
      out += ' #1'
      return
    }

    if (f.$field === 'ancestors') {
      out += ' #1'
      return
    }

    if (f.$field === 'id') {
      out += ' #1'
      findIn = <string[]>f.$value
      return
    }

    if (f.$operator == 'exists') {
      const fieldId = regIndex
      reg[regIndex++] = f.$field

      out += ` $${fieldId} h`
      return
    } else if (f.$operator == 'notExists') {
      const fieldId = regIndex
      reg[regIndex++] = f.$field

      out += ` $${fieldId} h L`
      return
    }

    const vType = getValueType(f)
    if (!ignoreLang && vType === 'string' && language) {
      ast2rpnFork(
        {
          isFork: true,
          $or: [
            {
              $operator: f.$operator,
              $field: f.$field,
              $value: f.$value
            },
            {
              $operator: f.$operator,
              $field: f.$field + '.' + language,
              $value: f.$value
            }
          ]
        },
        true
      )
      return
    } else if (vType == 'string' || vType == 'number') {
      const fieldId = regIndex
      reg[regIndex++] = f.$field

      const valueId = regIndex

      if (
        typeof f.$value === 'string' &&
        f.$value.startsWith('now') &&
        (f.$operator === '<' || f.$operator === '>')
      ) {
        reg[regIndex++] = `${convertNow(<string>f.$value)}`
      } else {
        reg[regIndex++] = `${f.$value}`
      }

      const op =
        vType == 'string' ? opMapString[f.$operator] : opMapNumber[f.$operator]
      if (!op) {
        console.error(`Invalid op for ${vType} field`, f)
      }

      if (vType === 'number') {
        out += ` @${valueId} $${fieldId} g ${op}`
      } else {
        out += ` $${valueId} $${fieldId} f ${op}`
      }
    } else if (vType == 'boolean') {
      const fieldId = regIndex
      reg[regIndex++] = f.$field
      const valueId = regIndex
      reg[regIndex++] = f.$value ? '1' : '0'

      const op = opMapNumber[f.$operator]
      if (!op) {
        console.error('Invalid op for boolean field', f)
        // TODO error
      }

      out += ` @${valueId} $${fieldId} g ${op}`
    } else if (f.$operator == '..') {
      const fieldId = regIndex
      reg[regIndex++] = f.$field
      const valueId1 = regIndex

      if (typeof f.$value[0] === 'string' && f.$value[0].startsWith('now')) {
        reg[regIndex++] = `${convertNow(<string>f.$value[0])}`
      } else {
        reg[regIndex++] = `${f.$value[0]}`
      }

      const valueId2 = regIndex
      if (typeof f.$value[1] === 'string' && f.$value[1].startsWith('now')) {
        reg[regIndex++] = `${convertNow(<string>f.$value[1])}`
      } else {
        reg[regIndex++] = `${f.$value[1]}`
      }

      out += ` @${valueId2} $${fieldId} g @${valueId1} i`
    } else if (vType === 'object') {
      const fork: Fork = {
        isFork: true
      }
      if (f.$operator === '!=') {
        fork.$and = []

        const arr = <(string | number)[]>f.$value
        for (let i = 0; i < arr.length; i++) {
          fork.$and[i] = {
            $field: f.$field,
            $operator: f.$operator,
            $value: arr[i]
          }
        }
      } else {
        fork.$or = []

        const arr = <(string | number)[]>f.$value
        for (let i = 0; i < arr.length; i++) {
          fork.$or[i] = {
            $field: f.$field,
            $operator: f.$operator,
            $value: arr[i]
          }
        }
      }

      ast2rpnFork(fork)
    } else {
      // TODO error
      console.error('Type error', vType, f)
    }
  }

  function ast2rpnFork(expr: Fork, ignoreLang: boolean = false) {
    const lop: ' M' | ' N' = expr.$and ? ' M' : ' N'
    const arr = expr.$and || expr.$or || []

    for (let i = 0; i < arr.length; i++) {
      const el = arr[i]

      if (isFork(el)) {
        ast2rpnFork(el)
      } else {
        ast2rpnFilter(el, ignoreLang)
      }

      if (i > 0) {
        out += lop
      }
    }
  }

  ast2rpnFork(f)

  const res = [out]
  for (let i = 1; i < regIndex; i++) {
    res[i] = reg[i]
  }
  return [findIn, res]
}
