import { TraverseByType, TraverseByTypeExpression } from './types'

function expr2rpn(
  types: Record<string, { prefix?: string }>,
  rule: TraverseByTypeExpression
): string {
  return typeof rule === 'string'
    ? `{"${rule}"}`
    : rule === false
    ? '{}'
    : rule.$all
    ? all2rpn(types, { $all: rule.$all })
    : first2rpn(types, { $first: rule.$first })
}

function all2rpn(
  types: Record<string, { prefix?: string }>,
  t: { $all: TraverseByTypeExpression[] }
): string {
  let result = ''
  for (let i = 0; i < t.$all.length; i++) {
    const expr = t.$all[i]
    result += ' ' + expr2rpn(types, expr) + (i > 0 ? ' z' : '')
  }

  return result.slice(1)
}

function first2rpn(
  types: Record<string, { prefix?: string }>,
  t: { $first: TraverseByTypeExpression[] }
): string {
  let result = ''
  for (let i = 0; i < t.$first.length; i++) {
    const expr = t.$first[i]
    result += ' ' + expr2rpn(types, expr) + (i > 0 ? ' z' : '')
  }

  result += ' j'

  return result.slice(1)
}

export default function bfsExpr2rpn(
  types: Record<string, { prefix?: string }>,
  t: TraverseByType
): string {
  let label = 1
  let rpn: string[] = []

  for (const typeName in t) {
    if (typeName === '$any') {
      continue
    }

    const rule = t[typeName]
    const valueExpr = expr2rpn(types, rule)
    const typePrefix = typeName === 'root' ? 'ro' : types[typeName].prefix

    rpn.push(`"${typePrefix}" e L >${label} ${valueExpr} Z .${label}`)
    label++
  }

  return `${rpn.join(':')}:${expr2rpn(types, t.$any)}`
}
