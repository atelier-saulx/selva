import { join as pathJoin } from 'path'
import { readFileSync } from 'fs'

export let SCRIPTS: Record<string, { content: string; sha: string }>

try {
  SCRIPTS = ['update-schema'].reduce((obj, scriptName) => {
    let distPath = pathJoin(__dirname, '..', '..', '..', 'client')
    if (!distPath.endsWith('dist')) {
      distPath = pathJoin(distPath, 'dist')
    }
    return Object.assign(obj, {
      [scriptName]: {
        content: readFileSync(
          pathJoin(distPath, 'lua', `${scriptName}.lua`),
          'utf8'
        ),
      },
    })
  }, {})
} catch (err) {
  console.error(`Failed to read lua scripts ${err.path}`)
  process.exit(1)
}
