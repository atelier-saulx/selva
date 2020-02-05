// import { Schema } from '~selva/schema/index'
// import { QeuryResult, FilterAST } from './types'
// import { isArray, indexOf } from '../../util'
// import * as logger from '../../logger'

// const filterExists = (a: FilterAST, b: FilterAST): boolean => {
//   return (
//     !a.$or &&
//     a.$operator === b.$operator &&
//     a.$field === b.$field &&
//     a.$value === b.$value
//   )
// }

// const isRelation = (field: string, _schema: Schema): boolean => {
//   // prob want to add more stuff
//   return field === 'ancestors' || field === 'children' || field === 'parents'
// }

// const isEqual = (
//   a: FilterAST,
//   b: FilterAST,
//   schema: Schema
// ): [boolean, null | string] => {
//   // something is wrong here
//   if (a.$value !== b.$value && type(a.$value) !== type(b.$value)) {
//     if (isArray(a.$value)) {
//       if (indexOf(a.$value, b.$value) !== -1) {
//         return [false, null]
//       }
//     } else if (isArray(b.$value)) {
//       if (indexOf(b.$value, a.$value) !== -1) {
//         b.$value = a.$value
//         return [false, null]
//       }
//     }
//   } else if (isRelation(a.$field, schema)) {
//     compareFilters
//     let bArray: (number | string)[] = []
//     if (!isArray(b.$value)) {
//       bArray = b.$value = [b.$value]
//     } else {
//       bArray = <(number | string)[]>b.$value
//     }
//     bArray[bArray.length] = <number | string>a.$value
//     return [false, null]
//   } else if (a.$value !== b.$value) {
//     return [
//       false,
//       `Cannot have 2 isEqual conditions @${a.$field} (${a.$value}) and (${b.$value})`
//     ]
//   }
//   return [true, null]
// }

// const isLargerThenAndSmallerThen = (
//   a: FilterAST,
//   b: FilterAST,
//   _schema: Schema
// ): [boolean, null | string] => {
//   let $lo, $hi
//   if (a.$operator === '>') {
//     $lo = a
//     $hi = b
//   } else {
//     $lo = b
//     $hi = a
//   }
//   a.$value = [$lo.$value, $hi.$value]
//   a.$operator = '..'
//   return [false, null]
// }

// const isLargerThenAndLargerThen = (
//   a: FilterAST,
//   b: FilterAST,
//   _schema: Schema
// ): [boolean, null | string] => {
//   if (b.$value > a.$value) {
//     a.$value = b.$value
//   }
//   return [false, null]
// }

// const isSmallerThenAndSmallerThen = (
//   a: FilterAST,
//   b: FilterAST,
//   _schema: Schema
// ): [boolean, null | string] => {
//   if (b.$value < a.$value) {
//     a.$value = b.$value
//   }
//   return [false, null]
// }

// const isRangeAndLargerOrSmaller = (
//   a: FilterAST,
//   b: FilterAST,
//   _schema: Schema
// ): [boolean, null | string] => {
//   let range = a
//   let other = b
//   if (b.$operator === '..') {
//     range = b
//     other = a
//   }
//   if (other.$operator === '>') {
//     if (other.$value > range.$value[1]) {
//       return [
//         false,
//         `Out of bounds range filter ${other.$value} < ${range.$value}`
//       ]
//     }
//   }
//   if (other.$operator === '<') {
//     if (other.$value > range.$value[0]) {
//       return [
//         false,
//         `Out of bounds range filter ${other.$value} > ${range.$value}`
//       ]
//     }
//   }
//   if (b.$operator === '..') {
//     a.$operator = '..'
//     a.$value = b.$value
//   }
//   return [false, null]
// }

// const isNotEqual = (
//   a: FilterAST,
//   b: FilterAST,
//   _schema: Schema
// ): [boolean, null | string] => {
//   if (a.$value !== b.$value) {
//     if (!isArray(b.$value)) {
//       if (!isArray(a.$value)) {
//         a.$value = [a.$value, b.$value]
//       } else if (indexOf(a.$value, b.$value) === -1) {
//         a.$value[a.$value.length] = b.$value
//       }
//     } else {
//       if (!isArray(a.$value)) {
//         if (indexOf(b.$value, a.$value) !== -1) {
//           a.$value = b.$value
//         } else {
//           b.$value[b.$value.length] = a.$value
//           a.$value = b.$value
//         }
//       } else {
//         for (let i = 0; i < b.$value.length; i++) {
//           if (indexOf(a.$value, b.$value[i]) === -1) {
//             a.$value[a.$value.length] = b.$value[i]
//           }
//         }
//       }
//     }
//   }
//   return [false, null]
// }

// const isNotEqualAndIsEqual = (
//   a: FilterAST,
//   b: FilterAST,
//   _schema: Schema
// ): [boolean, null | string] => {
//   if (a.$value === b.$value) {
//     return [
//       false,
//       `Cannot have something equal and inequal @${a.$field} (${a.$operator}${a.$value}) and (${b.$operator}${b.$value})`
//     ]
//   }
//   return [false, null]
// }

// const compareFilters = (
//   result: QeuryResult,
//   filter: FilterAST,
//   schema: Schema
// ): [false | FilterAST, string | null] => {
//   if (!filter.$field) {
//     return [false, 'No field incorrect args']
//   }

//   const a = result.reverseMap[filter.$field]
//   if (!a) {
//     result.reverseMap[filter.$field] = []
//     return [filter, null]
//   }

//   if (filter.$or) {
//     return [filter, null]
//   }

//   for (let i = 0; i < a.length; i++) {
//     const prevFilter = a[i]
//     if (filterExists(prevFilter, filter)) {
//       logger.info('filter exists??')

//       return [false, null]
//     }

//     const $a = prevFilter.$operator
//     const $b = filter.$operator

//     let fn:
//       | undefined
//       | ((a: any, b: any, schema: Schema) => [boolean, string | null])

//     if ($a === '=' && $b === '=') {
//       fn = isEqual
//     } else if ($a === '!=' && $b === '!=') {
//       fn = isNotEqual
//     } else if (($a === '=' && $b === '!=') || ($a === '!=' && $b === '=')) {
//       fn = isNotEqualAndIsEqual
//     } else if (($a === '>' && $b === '<') || ($a === '<' && $b === '>')) {
//       fn = isLargerThenAndSmallerThen
//     } else if ($a === '>' && $b === '>') {
//       fn = isLargerThenAndLargerThen
//     } else if ($a === '<' && $b === '<') {
//       fn = isSmallerThenAndSmallerThen
//     } else if (
//       // auto merge smaller then and equal then arrays or just dont allow them
//       ($a === '..' && ($b === '>' || $b === '<')) ||
//       ($b === '..' && ($a === '>' || $a === '<'))
//     ) {
//       fn = isRangeAndLargerOrSmaller
//     }

//     if (!fn) {
//       return [false, 'Cannot find parser']
//     }

//     const [r, err] = fn(prevFilter, filter, schema)

//     if (err) {
//       return [false, err]
//     } else if (r === false) {
//       return [false, null]
//     }
//   }

//   return [filter, null]
// }

// export default compareFilters
