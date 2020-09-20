console.log('hello')
const testAddon = require('../../build/Release/testaddon.node')

console.log('????', testAddon)

console.log(testAddon.hello('x', 'y'))
// include the c bindings

export const diff = () => {}

// apply diff is browser as well (important for hub etc)
export const applyDiff = () => {}
