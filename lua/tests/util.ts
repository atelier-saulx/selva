import {
  arrayIsEqual,
  isArray,
  joinString,
  splitString,
  ensureArray
} from '../src/util'

export function test_arrayIsEqual() {
  testy_assert(arrayIsEqual([1, 2, 3], [1, 2, 3]), '[1,2,3] === [1,2,3]')
  testy_assert(!arrayIsEqual([1, 2, 3], [1, 2]), '[1,2,3] !== [1,2]')
  testy_assert(!arrayIsEqual([1, 2, 3], [1, 3, 2]), '[1,2,3] !== [1,3,2]')
}

export function test_isArrayWithPrimitives() {
  testy_assert(isArray(true) === false, '[true] not array')
  testy_assert(isArray(123) === false, '123 not array')
  testy_assert(isArray('hello') === false, '"hello" not array')
}

export function test_isArrayWithObjects() {
  testy_assert(isArray({ a: 1, b: 2, c: 3 }) === false, 'object not array')
  // testy_assert(isArray({}) === false) // this is an unfortunate case we can't really detect, but we can always assume it's an empty array in these cases
}

export function test_isArrayWithArray() {
  testy_assert(isArray([1, 2]) === true, '[1,2] is array')
  testy_assert(isArray([1]) === true, '[1] is array')
  testy_assert(isArray([]) === true, '[] is array')
}

export function test_joinString() {
  testy_assert(
    joinString(['lekker', 'man'], '|') === 'lekker|man',
    'can join array with multiple elements'
  )
  testy_assert(
    joinString(['sick'], '|') === 'sick',
    'can join array with single element'
  )
  testy_assert(joinString([], '|') === '', 'can join empty array')
}

export function test_splitString() {
  testy_assert(
    arrayIsEqual(splitString('lekker man', ' '), ['lekker', 'man']),
    'can split string with 2 split results'
  )
  testy_assert(
    arrayIsEqual(splitString('lekker', ' '), ['lekker']),
    'can split string with nothing to split but has data'
  )
  testy_assert(arrayIsEqual(splitString('', ' '), []), 'can split empty string')
}

export function test_ensureArrayWithArray() {
  const ary1 = [1]
  const ary2 = [1, 2]

  testy_assert(ensureArray(ary1) === ary1)
  testy_assert(ensureArray(ary2) === ary2)
}

export function test_ensureArrayWithNonArrays() {
  const str = ensureArray('lekker man')
  testy_assert(str.length === 1 && str[0] === 'lekker man')

  const num = ensureArray(12)
  testy_assert(num.length === 1 && num[0] === 12)

  const bool = ensureArray(false)
  testy_assert(bool.length === 1 && bool[0] === false)

  const obj = { a: 1, b: 2 }
  const objAry = ensureArray(obj)
  testy_assert(objAry.length === 1 && objAry[0] === obj)
}
