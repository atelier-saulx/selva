const compareString = () => {}

// TYPE 0 === set, TYPE 1 === string, type 2 === remove, type 3 === insert (array)
// [path, TYPE, VALUE]

// tree better?
// probably - less reuse

// something like this
const PATCH = '_$P'

/*

// if array can have "0" etc in object


___$patch___ = []
{
  bla: 'yesh,
  blurf: {
      blap: 'surl',
      x: [] // array means this is a patch can be a set with a value
  }


}

// if (testArray.constructor === Array) result = true; (fastest)
*/

const compare = (a, b, result, key) => {
  const type = typeof b

  // eslint-disable-next-line
  if (type !== typeof a) {
    // different type good start
    // strong
    console.info('type is different', a, b)
    if (!key) {
      return b
    } else {
      result[key] = b
    }
  } else if (type === 'object') {
    if (b === null) {
      // add changez
      //   result
      //   result.push([path, 0, b])
      if (!key) {
        return b
      } else {
        result[key] = b
      }
    } else {
      if (b.constructor === Array) {
        if (a.constructor === Array) {
          // check the differences!
        } else {
          // convert to array make different
          if (!key) {
            return [0, b]
          } else {
            result[key] = [0, b]
          }
        }
      } else {
        // yesh its an object now find the rest

        // double go time
        for (const key in b) {
          if (!(key in a)) {
            result[key] = [0, b[key]]
          } else {
            // same for a need to remove keys if b does not have them
            compare(a[key], b[key], result, key)
          }
        }

        for (const key in a) {
          if (!(key in b)) {
            result[key] = [2]
          }
        }
      }
    }
  } else {
    if (a === b) {
      // no change do nothing
    } else {
      if (type === 'string') {
      } else {
        // add change
      }
    }
  }
  // either array or object
  // if string special handle

  console.log('compare', a, b)
  return result
}

// const compareTop = (a, b ) => {

// }

const applyStringPatch = () => {}

const applyPatch = () => {}

exports.compare = compare
exports.compareString = compareString
