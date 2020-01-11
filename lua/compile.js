const fs = require('fs').promises
const path = require('path')

async function runTemplating(contents) {
  const pathToContent = {}

  const matches = contents.match(/--\s*%import\s*\w+\s*from\s*"(.+)"/gi)
  for (match of matches) {
    const modulePath = match.match(/"(.+)"/i)[1]
    pathToContent[modulePath] = await fs.readFile(
      path.join(__dirname, 'scripts', modulePath),
      'utf8'
    )
  }

  return contents.replace(
    /--\s*%import\s*(\w+)\s*from\s*"(.+)"/gi,
    (match, fnName, modulePath) => {
      return `
local _${fnName} = function()
${pathToContent[modulePath]}
end
local ${fnName} = _${fnName}() -- resolve the import        `
    }
  )
}

async function run() {
  const scripts = await fs.readdir(path.join(__dirname, 'scripts'))
  await Promise.all(
    scripts.map(async script => {
      const content = await fs.readFile(
        path.join(__dirname, 'scripts', script),
        'utf8'
      )
      const compiled = await runTemplating(content)
      await fs.writeFile(path.join(__dirname, 'dist', script), compiled)
    })
  )
}

run().catch(e => {
  console.error(e)
  process.exit(1)
})
