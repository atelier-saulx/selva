import { Schema } from '../../'
import { GetOperationAggregate, GetOperationFind } from '../types'
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
      } else if (typeof f.$value === 'number') {
        return `#${f.$value} "${f.$field}" a`
      } else if (Array.isArray(f.$value)) {
        if (typeof f.$value[0] === 'string') {
          if (f.$value.some((v: string) => v.includes('"'))) {
            // We can't inline quotes at the moment
            return null
          }

          const a = `{${f.$value.map((v) => `"${v}"`).join(',')}}`
          return `"${f.$field}" ${a} l`
        } else if (typeof f.$value[0] === 'number') {
          const a = `{${f.$value.map((v) => `#${v}`).join(',')}}`
          return `"${f.$field}" ${a} l`
        }
        return null
      }
    case 'exists':
      return `"${f.$field}" h`
    case 'notExists':
      return `"${f.$field}" h L`
  }

  return null
}

const nonIndexedFields = new Set(['node', 'ancestors'])

export function mkIndex(schema: Schema, op: GetOperationFind | GetOperationAggregate): string[] {
  if (!op.filter || !op.filter.$and) {
    return []
  }

  if (typeof op.sourceField !== 'string' || nonIndexedFields.has(op.sourceField)) {
    return []
  }

  return op.filter.$and
    .filter((f: Fork | FilterAST) => !isFork(f))
    .map((f: FilterAST) => ast2inlineRpn(schema, f))
    .filter((s: string | null) => s)
    .map((s: string): string[] => ['index', s])
    .reduce((prev, cur) => (prev.push(...cur), prev), [])
}
