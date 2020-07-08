const testAddon = require('./build/Release/testaddon.node')
console.log('hello ', testAddon.hello('x', 'y'))

module.exports = testAddon
