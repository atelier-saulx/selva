// import { QeuryResult, FilterAST } from './types'

// const addToResult = (result: QeuryResult, filter: FilterAST) => {
//   const field = filter.$field
//   if (!result.filters.$and) {
//     result.filters.$and = []
//   }
//   const f = result.filters.$and
//   f[f.length] = filter
//   if (!result.reverseMap[field]) {
//     result.reverseMap[field] = []
//   }
//   const r = result.reverseMap[field]
//   r[r.length] = filter
// }

// export default addToResult
