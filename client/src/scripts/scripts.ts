import { readFileSync } from 'fs'
import { join as pathJoin } from 'path'

let SCRIPTS

try {
  SCRIPTS = ['modify', 'fetch', 'id', 'update-schema'].reduce(
    (obj, scriptName) => {
      let distPath = pathJoin(__dirname, '..')
      if (!distPath.endsWith('dist')) {
        distPath = pathJoin(distPath, 'dist')
      }
      return Object.assign(obj, {
        [scriptName]: readFileSync(
          pathJoin(distPath, 'lua', `${scriptName}.lua`),
          'utf8'
        )
      })
    },
    {}
  )
} catch (e) {
  console.error(`Failed to read modify.lua ${e.stack}`)
  process.exit(1)
}
