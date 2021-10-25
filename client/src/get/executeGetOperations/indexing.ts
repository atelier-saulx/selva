import { Schema } from '../../'
import { GetOperationFind } from '../types'
import {
  Fork,
  FilterAST,
  isFork,
} from '@saulx/selva-query-ast-parser'

function ast2inlineRpn(schema: Schema, f: FilterAST | null): string | null {
  if (!f) {
    return null
  }

  if (f.$field === 'type') {
    const prefix =
      f.$value === 'root' ? 'ro' : schema?.types[<string>f.$value]?.prefix
    if (!prefix) {
      return null
    }

    if (f.$operator == '=') {
      return `"${prefix}" e`
    } else if (f.$operator == '!=') {
      return `"${prefix}" e L`
    }
    return null
  }

  switch (f.$operator) {
    case '=':
      if (typeof f.$value === 'string') {
        return `"${f.$field}" f "${f.$value}" c`
      } else {
        // numeric
        const num = Number(f.$value)
        return Number.isNaN(num) ? null : `"${f.$field}" g #${num} F`
      }
    case 'has':
      if (typeof f.$value === 'string') {
        return `"${f.$value}" "${f.$field}" a`
      } else {
        // numeric
        return `#${f.$value} "${f.$field}" a`
      }
    case 'exists':
      return `"${f.$field}" h`
    case 'notExists':
      return `"${f.$field}" h L`
  }

  return null
}

export function mkIndex(schema: Schema, op: GetOperationFind): string[] {
  if (op.id && op.id !== 'root') {
    return []
  }

  if (!op.filter || !op.filter.$and) {
    return []
  }

  if (op.sourceField !== 'descendants') {
    return []
  }

  return op.filter.$and
    .filter((f: Fork | FilterAST) => !isFork(f))
    .map((f: FilterAST) => ast2inlineRpn(schema, f))
    .filter((s: string | null) => s)
    .map((s: string): string[] => ['index', s])
    .reduce((prev, cur) => (prev.push(...cur), prev), [])
}
