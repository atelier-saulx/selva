import { arrayIsEqual } from '../lua/src/util'

export function test_arrayIsEqual() {
  assert(arrayIsEqual([1, 2, 3], [1, 2, 3]))
  assert(!arrayIsEqual([1, 2, 3], [1, 2]))
  assert(!arrayIsEqual([1, 2, 3], [1, 3, 2]))
}
