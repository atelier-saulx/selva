import test from 'ava'
import { parseFilters, printAst } from '../src'
import { Filter, GeoFilter } from '../../client/src/get/types'

test('basic filter', async t => {
  console.log('go go go')

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

  const ast = parseFilters(filter)

  console.log({ ast })

  t.pass()
})
