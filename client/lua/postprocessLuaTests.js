const fs = require('fs').promises
const path = require('path')

const CWD = process.cwd()

const filename = path.join(CWD, 'dist', 'lua', 'tests.lua')

fs.readFile(filename, 'utf8')
  .then(content => {
    const moduleTableLocation = content.indexOf('____modules = {\n')
    const returnIdx = content.indexOf('return require("luaScripts.tests")')

    const withoutRequireShimAndReturn = content.substring(
      moduleTableLocation,
      returnIdx
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

    let testFunctions = `
          local ___tests = require("luaScripts.tests")`
    for (const match of withoutRequireShimAndReturn.matchAll(
      /____exports\.test_(\w+)\(self\)/g
    )) {
      const testName = match[1]
      testFunctions += `
      local function test_${testName}()
        ___tests.test_${testName}()
      end`
    }

    return fs.writeFile(
      filename,
      newRequireOverride + withoutRequireShimAndReturn + testFunctions
    )
  })
  .catch(e => {
    console.error(e)
    process.exit(1)
  })
