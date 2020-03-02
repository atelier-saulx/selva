#!/usr/bin/env node
const chokidar = require('chokidar')
const cp = require('child_process')
const execa = require('execa')
const path = require('path')

function update() {
  if (!currentlyBuilding) {
    currentlyBuilding = true
    ava.stdin.write('r\n', 'utf8')
  }
}

const files = process.argv.slice(2).map(f => {
  const parts = f.split(path.sep)
  let first = parts[0]
  if (first === '.') {
    first = parts[1]
  }

  if (first === 'client') {
    return parts[0] === '.'
      ? path.join(...parts.slice(2))
      : path.join(...parts.slice(1))
  }

  return f
})
console.log('FILES', files)

let currentlyBuilding = true

async function handleLua() {
  const { stdout, stderr } = await execa('yarn', ['buildLua'])
  console.log(stdout)
  if (stderr && stderr.length) {
    console.error(stderr)
  }

  update()
}

function attachChokidarHandlers(watcher) {
  watcher.on('change', handleLua)
  watcher.on('unlink', handleLua)
  watcher.on('error', handleLua)
}

console.log('yarn', ['watchClientTests'].concat(files))
const ava = cp.spawn('yarn', ['watchClientTests'].concat(files))
ava.stdout.on('data', d => {
  const str = d.toString()
  if (str.includes('and press enter to rerun tests')) {
    currentlyBuilding = false
  }

  console.log('[ava]', str)
})

ava.stderr.on('data', d => {
  console.error('[ava]', d.toString())
})

const server = cp.spawn('yarn', ['watchServer', '--preserveWatchOutput'])

server.stdout.on('data', d => {
  const str = d.toString()
  console.log('[tsc]', str)

  if (str.includes('Found 0 errors. Watching for file changes')) {
    update()
  }
})

server.stderr.on('data', d => {
  console.error('[tsc]', d.toString())
})

const luaWatcher = chokidar.watch(
  path.join(__dirname, '..', 'client', 'lua', 'src')
)
const luaScriptsWatcher = chokidar.watch(
  path.join(__dirname, '..', 'client', 'luaScripts')
)

attachChokidarHandlers(luaWatcher)
attachChokidarHandlers(luaScriptsWatcher)
