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
  let rpn = ''
  for (const typeName in t) {
    if (typeName === '$any') {
      continue
    }

    // set up value expr
    const rule = t[typeName]
    const valueExpr = expr2rpn(types, rule)

    const typePrefix = types[typeName].prefix
    const ternSegment = ` ${valueExpr} ${typePrefix} e T`

    rpn = ternSegment + rpn
  }

  const $any: TraverseByTypeExpression = t.$any
  const anyExpr = expr2rpn(types, $any)
  rpn = anyExpr + rpn
  return rpn
}
