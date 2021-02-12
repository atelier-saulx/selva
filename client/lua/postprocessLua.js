const fs = require('fs').promises
const path = require('path')

const CWD = process.cwd()

const filename = process.argv[2]

if (!filename) {
  console.error('No filename provided')
  process.exit(1)
}

fs.readFile(path.join(CWD, filename), 'utf8')
  .then((content) => {
    const moduleTableLocation = content.indexOf('____modules = {\n')
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
      path.join(CWD, filename),
      newRequireOverride + withoutRequireShim
    )
  })
  .catch((e) => {
    console.error(e)
    process.exit(1)
  })
