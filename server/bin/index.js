#!/usr/bin/env node
const { start } = require('../')
const [, , ...args] = process.argv

const options = {
  '-p': 'port',
  '--port': 'port',
  '-r': 'replica',
  '-replica': 'replica'
}
const opts = {}

for (let i = 0; i < args.length - 1; i += 2) {
  const option = options[args[i]]
  if (option) {
    opts[option] = args[i + 1]
  }
}

start(opts)
