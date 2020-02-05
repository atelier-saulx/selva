// import { Schema } from '~selva/schema/index'
// import { isArray, joinString } from '../../util'
// import { FilterAST } from './types'
// import * as logger from '../../logger'

// // do this last
// const returnNumber = (filter, value: string): string => {
//   if (filter.$operator === '>') {
//     // depending on schema...
//     return `(@${filter.$field}:[${value},inf])`
//   } else if (filter.$operator === '..') {
//     return `(@${filter.$field}:[${value[0]},${value[1]}])`
//   } else if (filter.$operator === '!=') {
//     return `(-(@${filter.$field}:[${value},${value}]))`
//   } else if (filter.$operator === '=') {
//     return `(@${filter.$field}:[${value},${value}])`
//   }
//   return ''
// }

// // ADD TEXT AND GEO
// // SEARCH DB
// // LEVEN STEIN DISTANCE index language
// // SEARCH PREFIXES
// // also incolude language in searching REAL SEARCH

// const addField = (filter: FilterAST, _schema: Schema): string => {
//   // depends on field type
//   const type = filter.$search && filter.$search[0]
//   const operator = filter.$operator
//   if (type === 'TAG') {
//     if (isArray(filter.$value)) {
//       filter.$value = `${joinString(<string[]>filter.$value, '|')}`
//     }
//     if (operator === '!=') {
//       return `(-(@${filter.$field}:{${filter.$value}}))`
//     } else if (operator === '=') {
//       return `(@${filter.$field}:{${filter.$value}})`
//     }
//   } else if (type === 'NUMERIC') {
//     if (isArray(filter.$value) && filter.$operator !== '..') {
//       let value = ''
//       for (let i = 0, len = filter.$value.length; i < len; i++) {
//         const v = returnNumber(filter, tostring(filter.$value[i]))
//         value += value ? '|' + v : v
//       }
//       return `(${value})`
//     } else {
//       return returnNumber(filter, tostring(filter.$value))
//     }
//   } else if (type === 'TEXT') {
//     // equals will be a partial here
//     // DO THINGS
//     // INCLUDE LANGUAGE ETC
//   } else if (type === 'GEO') {
//     // later
//   }
//   return ''
// }

// // for of

// // type Filters = Filter[]

// function createSearchString(
//   filters: FilterAST,
//   schema: Schema
// ): [string, string | null] {
//   const searchString: string[] = []

//   if (filters.$and && filters.$or) {
//     return ['', 'cannot have $or and $and on one intermediate result level']
//   }

//   if (filters.$and) {
//     for (let filter of filters.$and) {
//       if (!filter.$or) {
//         searchString[searchString.length] = addField(filter, schema)
//       } else {
//         const [nestedSearch, err] = createSearchString(filter, schema)
//         if (err) {
//           return ['', err]
//         }
//         searchString[searchString.length] = nestedSearch
//       }
//     }
//     return [`(${joinString(searchString, ' ')})`, null]
//   } else if (filters.$or) {
//     for (let filter of filters.$or) {
//       if (!filter.$and) {
//         if (filter.$or) {
//           const [nestedSearch, err] = createSearchString(filter, schema)
//           if (err) {
//             return ['', err]
//           }
//           searchString[searchString.length] = nestedSearch
//         } else {
//           searchString[searchString.length] = addField(filter, schema)
//         }
//       } else {
//         const [nestedSearch, err] = createSearchString(filter, schema)
//         if (err) {
//           return ['', err]
//         }
//         searchString[searchString.length] = nestedSearch
//       }
//     }
//     return [`(${joinString(searchString, '|')})`, null]
//   }
//   return ['', 'No valid cases for createSearchString']
// }

// export default createSearchString
