import * as logger from '../../logger'
import { Fork, FilterAST as Filter } from './types'
import { now } from '../../util'

function isFork(x: any): x is Fork {
  return x.isFork
}

function getValueType(f: Filter) {
  if (f.$operator == '<' || f.$operator == '>') {
    return 'number'
  }
  return type(f.$value)
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

export default function ast2rpn(f: Fork): [string[] | undefined, string[]] {
  let findIn: string[] | undefined = undefined
  let out = ''
  let reg: string[] = []
  let regIndex = 1

  function ast2rpnFilter(f: Filter) {
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
    if (vType == 'string' || vType == 'number') {
      const fieldId = regIndex
      reg[regIndex++] = f.$field
      const valueId = regIndex

      if (f.$value === 'now' && (f.$operator === '<' || f.$operator === '>')) {
        reg[regIndex++] = `${now()}`
      } else {
        reg[regIndex++] = `${f.$value}`
      }

      const op =
        vType == 'string' ? opMapString[f.$operator] : opMapNumber[f.$operator]
      if (!op) {
        logger.error('Invalid op', f)
        // TODO error
      }

      if (vType === 'number') {
        out += ` @${valueId} $${fieldId} g ${op}`
      } else {
        out += ` $${valueId} $${fieldId} f ${op}`
      }
    } else if (f.$operator == '..') {
      const fieldId = regIndex
      reg[regIndex++] = f.$field
      const valueId1 = regIndex
      reg[regIndex++] = `${f.$value[0]}`
      const valueId2 = regIndex
      reg[regIndex++] = `${f.$value[1]}`

      out += ` @${valueId2} $${fieldId} g @${valueId1} i`
    } else if (vType === 'table') {
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
            $value: arr[i],
            $search: []
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
            $search: []
          }
        }
      }

      ast2rpnFork(fork)
    } else {
      // TODO error
      logger.error('Type error')
    }
  }

  function ast2rpnFork(expr: Fork) {
    const lop: ' M' | ' N' = expr.$and ? ' M' : ' N'
    const arr = expr.$and || expr.$or || []

    for (let i = 0; i < arr.length; i++) {
      const el = arr[i]

      if (isFork(el)) {
        ast2rpnFork(el)
      } else {
        ast2rpnFilter(el)
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
