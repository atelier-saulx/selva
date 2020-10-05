import test from 'ava'
import { createAst, printAst, createRpn } from '../src'
import { Filter, GeoFilter } from '../../client/src/get/types'

test('basic filter', async t => {
  const filter: Filter[] = [
    {
      $field: 'type',
      $operator: '=',
      $value: 'team'
    },
    {
      $field: 'value',
      $operator: '!=',
      $value: 2
    }
  ]

  const ast = createAst(filter)

  t.deepEqual(ast, {
    isFork: true,
    $and: [
      { $value: 'team', $operator: '=', $field: 'type' },
      { $value: 2, $operator: '!=', $field: 'value' }
    ]
  })

  const rpn = createRpn(filter)

  printAst(ast)

  t.deepEqual(rpn, [
    undefined,
    [' $2 $1 f c @4 $3 g G M', 'type', 'team', 'value', '2']
  ])

  t.pass()
})
