const jimmers = (a, b) => {
  const aLen = a.length
  const bLen = b.length
  // 1 = insert, value
  // 3 = from , index, amount (can be a copy a well)
  // 4 = amount (means just repeat from which i you are at)
  const resultA = {}

  let resultB
  let aCalc

  if (bLen < aLen) {
    // optimized for removal of things in the array (mostly ad the end)
    let j = 0
    resultB = new Array(bLen)
    aCalc = new Array(aLen)
    let isDone = false
    for (let i = 0; i < bLen && !isDone; i++) {
      resultB[i] = b[i] // only for hash
      for (; j < aLen; j++) {
        const v = a[j]
        if (!resultA[v]) {
          resultA[v] = []
        }
        resultA[v].push(j)
        aCalc[j] = v
        if (j === aLen - 1) {
          isDone = true
          break
        }
        if (v === b[i]) {
          break
        }
      }
    }
    if (!isDone) {
      console.log('saved checking', aLen - j, 'entries!', 'checked ', j)
    }
  }

  const r = []
  let currRIndex = 0
  r[0] = bLen
  for (let i = 0; i < bLen; i++) {
    let av, bv
    if (aLen <= bLen) {
      if (i < aLen) {
        av = a[i]
        if (!resultA[av]) {
          resultA[av] = []
        }
        resultA[av].push(i)
      }
      bv = b[i]
    } else {
      av = aCalc[i]
      bv = resultB[i]
    }

    if (av === bv) {
      if (r[currRIndex] && r[currRIndex][0] === 4) {
        r[currRIndex][1]++
      } else {
        if (r[currRIndex] && r[currRIndex][0] === 3) {
          r[currRIndex][2] = r[currRIndex][2][0]
        }
        currRIndex++
        r[currRIndex] = [4, 1, i]
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
            if (nr2 + r[currRIndex][1] === nr) {
              f = nr2
              x = true
              break
            }
          }
          if (x) {
            break
          }
        }
      }
      if (f !== false) {
        r[currRIndex][1]++
        r[currRIndex][2] = [f]
      } else if (f === false) {
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

      if (r[currRIndex] && r[currRIndex][0] === 1) {
        r[currRIndex].push(bv)
      } else {
        currRIndex++
        r[currRIndex] = [1, bv]
      }
    }
  }
  if (r[r.length - 1][0] === 3) {
    r[r.length - 1][2] = r[currRIndex][2][0]
  }
  return r
}

const a = ['a', 'b', 'c', 'd']
const b = ['x', 'x', 'a', 'b', 'z', 'c', 'd']

console.log('DIFFY', a)
console.log('DIFFY2', b)
console.log(jimmers(a, b))

// lcs([1, 5, 8, 10], [7, 7, 1, 5, 3, 8, 10])

const largeArr = []

for (let i = 0; i < 10000; i++) {
  largeArr.push(i)
}

const largeArr2 = []

for (let i = 0; i < 10001; i++) {
  largeArr2.push(i)
}
largeArr2.splice(10, 0, 'flap')

// console.log(largeArr2)

var d = Date.now()
// console.log(largeArr, largeArr2)
const y = jimmers(largeArr, largeArr2)

// const y = [...calcPatch(largeArr, largeArr2)]
console.log(Date.now() - d, 'ms')

console.log(y)

const xx = [...largeArr]

xx[3] = 'flapperpaps'
for (let i = 0; i < 20; i++) {
  xx.push(i + 'x')
}

const yx = jimmers(largeArr, xx)
console.log(yx)

const bx = ['a', 'b', 'c', 'd']
const ax = ['x', 'x', 'a', 'b', 'z', 'c', 'd', 'e', 'f', 'g', 'urx']

console.log('DIFFYRD', ax)
console.log('DIFFY2ERS', bx)
console.log(jimmers(ax, bx))

export default jimmers
