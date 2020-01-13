const fs = require('fs').promises
const path = require('path')

let filename = process.argv[2]

if (!filename) {
  console.error('No filename provided')
  process.exit(1)
}

fs.readFile(path.join(__dirname, filename), 'utf8')
  .then(content => {
    let moduleTableLocation = content.indexOf('____modules = {\n')
    const withoutRequireShim = content.substring(
      moduleTableLocation,
      content.length
    )
    const newRequireOverride = `
  local ____modules = {}
  local ____moduleCache = {}
  local function require(file)
      if ____moduleCache[file] then
          return ____moduleCache[file]
      end
      if ____modules[file] then
          ____moduleCache[file] = ____modules[file]()
          return ____moduleCache[file]
      else
            error("module '" .. file .. "' not found")
      end
  end\n`

    return fs.writeFile(
      path.join(__dirname, filename),
      newRequireOverride + withoutRequireShim
    )
  })
  .catch(e => {
    console.error(e)
    process.exit(1)
  })
