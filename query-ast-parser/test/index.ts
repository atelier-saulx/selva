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
})

test('complex filter', async t => {
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
    },
    {
      $field: 'value',
      $operator: '=',
      $value: 3
    },
    {
      $field: 'flapdrol',
      $operator: '>',
      $value: 10
    },
    {
      $field: 'flapdrol',
      $operator: '>',
      $value: 100
    },
    {
      $field: 'x',
      $operator: '>',
      $value: 10
    },
    {
      $field: 'x',
      $operator: '>',
      $value: 100,
      $or: {
        $field: 'y',
        $operator: '=',
        $value: 'flapperdrol',
        $and: {
          $field: 'z',
          $operator: '=',
          $value: 'snurkypants'
        }
      }
    }
  ]

  const ast = createAst(filter)

  t.deepEqual(ast, {
    isFork: true,
    $and: [
      { $value: 'team', $operator: '=', $field: 'type' },
      { $value: 2, $operator: '!=', $field: 'value' },
      { $value: 100, $operator: '>', $field: 'flapdrol' },
      { $value: 10, $operator: '>', $field: 'x' },
      {
        isFork: true,
        $or: [
          { $value: 100, $operator: '>', $field: 'x' },
          {
            isFork: true,
            $and: [
              {
                $value: 'flapperdrol',
                $operator: '=',
                $field: 'y'
              },
              {
                $value: 'snurkypants',
                $operator: '=',
                $field: 'z'
              }
            ]
          }
        ]
      }
    ]
  })

  const rpn = createRpn(filter)

  printAst(ast)

  t.deepEqual(rpn, [
    undefined,
    [
      ' $2 $1 f c @4 $3 g G M @6 $5 g I M @8 $7 g I M @10 $9 g I $12 $11 f c $14 $13 f c M N M',
      'type',
      'team',
      'value',
      '2',
      'flapdrol',
      '100',
      'x',
      '10',
      'x',
      '100',
      'y',
      'flapperdrol',
      'z',
      'snurkypants'
    ]
  ])
})

test.only('exists & not exist', async t => {
  const filter: Filter[] = [
    {
      $field: 'type',
      $operator: 'exists'
    },
    {
      $field: 'flurp',
      $operator: 'notExists'
    }
  ]

  const ast = createAst(filter)
  printAst(ast)

  t.deepEqual(ast, {
    isFork: true,
    $and: [
      { $operator: 'exists', $field: 'type' },
      { $operator: 'notExists', $field: 'flurp' }
    ]
  })
})

test('reduce exists', async t => {
  const filter: Filter[] = [
    {
      $field: 'type',
      $operator: '=',
      $value: 'team'
    },
    {
      $field: 'type',
      $operator: 'exists'
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
})
