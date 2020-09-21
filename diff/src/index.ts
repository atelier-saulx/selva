import apply from './apply'
import create from './create'
// start with object diff!
import lcs from './lcs'

export const diff = create

export { apply, lcs }
// apply diff is browser as well (important for hub etc)
