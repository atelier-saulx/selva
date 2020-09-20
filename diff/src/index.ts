import apply from './apply'
import create from './create'

console.log('hello')
const testAddon = require('../build/Release/testaddon.node')

console.log('????', testAddon)

console.log(testAddon.hello('x', 'y'))
// include the c bindings

// start with object diff!

export const diff = create

export { apply }
// apply diff is browser as well (important for hub etc)
