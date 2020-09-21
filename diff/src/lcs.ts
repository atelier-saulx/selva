import deepNoEqual from './deepNotEqual'

function bt(
  c: any[],
  a: any[],
  b: any[],
  aLength: number,
  bLength: number
): any[] {
  if (aLength * bLength === 0) {
    return []
  } else if (a[aLength - 1] === b[bLength - 1]) {
    // nasty heavy this
    return [
      ...bt(c, a, b, aLength - 1, bLength - 1),
      { value: a[aLength - 1], from: aLength - 1, to: bLength - 1 }
    ]
  } else {
    return c[aLength][bLength - 1] > c[aLength - 1][bLength]
      ? bt(c, a, b, aLength, bLength - 1)
      : bt(c, a, b, aLength - 1, bLength)
  }
}

function lcs(a: any[], b: any[]) {
  const aLength = a.length
  const bLength = b.length
  const matrix = []

  // use this loop to go trought everything
  const result = []

  for (let i = 0; i <= aLength; i++) matrix.push([0])

  for (let j = 0; j < bLength; j++) matrix[0].push(0)

  //   console.log('------------------')

  for (let i = 0; i < aLength; i++) {
    for (let j = 0; j < bLength; j++) {
      matrix[i + 1][j + 1] =
        // also very heavy needs to be integrated
        deepNoEqual(a[i], b[j])
          ? Math.max(matrix[i + 1][j], matrix[i][j + 1])
          : matrix[i][j] + 1
    }
  }

  //   console.log('------------------')
  //   console.log('filled!', matrix)

  //   console.log('------------------')

  return bt(matrix, a, b, aLength, bLength)

  //   console.log(result)
}

// we dont want this at all
// what we need is checksums

// so the idea is we start with the common sequence

// then for each in the command sc we add stuff

// else we make a map with a counter for other values

// when using this means we want to store checksums at every object (for faster checks)

// only if an object we do this
// const hasher = (argHasher = (obj, hash = 5381) => {
//   for (let key in obj) {
//     const field = obj[key]
//     const type = typeof field
//     if (type === 'string') {
//       hash = (djb2(field, hash) * 33) ^ djb2(key, hash)
//     } else if (type === 'number') {
//       hash = (((hash * 33) ^ field) * 33) ^ djb2(key, hash)
//     } else if (type === 'object') {
//       if (field) {
//         hash = argHasher(field, hash)
//       }
//     } else if (type === 'boolean') {
//       hash = (((hash * 33) ^ (field === true ? 1 : 0)) * 33) ^ djb2(key, hash)
//     }
//   }
//   return hash
// })

const jimmers = (a, b) => {
  const aLen = a.length
  const bLen = b.length

  // 1 = insert, value
  // 3 = from , index, amount (can be a copy a well)
  // 2 = delete, amount
  // 4 = amount (means just repeat from which i you are at)

  // [[]]

  // als make duplaicate and option

  const r = []

  let currRIndex = -1

  const resultA = {}

  for (let i = 0; i < aLen; i++) {
    const v = a[i]
    if (!resultA[v]) {
      resultA[v] = []
    }
    resultA[v].push(i)
  }
  // a & b indexes
  // value : [[], []]

  if (bLen > aLen) {
    for (let i = 0; i < bLen; i++) {
      const av = a[i]
      const bv = b[i]
      if (av === bv) {
        if (r[currRIndex] && r[currRIndex][0] === 4) {
          r[currRIndex][1]++
        } else {
          if (r[currRIndex] && r[currRIndex][0] === 3) {
            r[currRIndex][2] = r[currRIndex][2][0]
          }
          currRIndex++
          r[currRIndex] = [4, 1]
        }
      } else if (resultA[bv]) {
        // getting closest match is very nice here
        let f = false
        if (r[currRIndex] && r[currRIndex][0] === 3) {
          // find 2 numbers close to each other
          for (let j = 0; j < resultA[bv].length; j++) {
            const nr = resultA[bv][j]
            const prev = r[currRIndex][2]
            let x = false
            for (let k = 0; k < prev.length; k++) {
              const nr2 = prev[k]
              if (nr2 + 1 === nr) {
                x = true
                break
              }
            }
            if (x) {
              f = nr
              break
            }
          }
        }
        if (f) {
          r[currRIndex][1]++
          r[currRIndex][2] = [f]
        } else if (!f) {
          if (r[currRIndex] && r[currRIndex][0] === 3) {
            r[currRIndex][2] = r[currRIndex][2][0]
          }

          currRIndex++
          r[currRIndex] = [3, 1, resultA[bv]]
        }
      } else {
        if (r[currRIndex] && r[currRIndex][0] === 3) {
          r[currRIndex][2] = r[currRIndex][2][0]
        }

        currRIndex++
        r[currRIndex] = [1, bv]
      }
    }

    if (r[r.length - 1][0] === 3) {
      r[r.length - 1][2] = r[currRIndex][2][0]
    }
  }

  return r

  // 2 ways wherte oyu start with b (if it shorter)
  // 1 where you styartt with a
}

const a = ['a', 'b', 'c', 'd']
const b = ['x', 'x', 'a', 'b', 'z', 'c', 'd']

console.log('DIFFY', a)
console.log('DIFFY2', b)
console.log(jimmers(a, b))

// lcs([1, 5, 8, 10], [7, 7, 1, 5, 3, 8, 10])

const largeArr = []

for (let i = 0; i < 1000; i++) {
  largeArr.push(i)
}

const largeArr2 = []

for (let i = 0; i < 1001; i++) {
  largeArr2.push(i)
}
largeArr2.splice(10, 0, 'flap')

// console.log(largeArr2)

var d = Date.now()
// console.log(largeArr, largeArr2)

const y = jimmers(largeArr, largeArr2)
console.log(Date.now() - d, 'ms')

console.log(y)

// console.log(lcs('dis a test bra manyo', 'this is a test, brother yo'))

export default lcs
