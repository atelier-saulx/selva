import { join as pathJoin } from 'path'
import { readFileSync } from 'fs'
import { Connection } from '.'

let SCRIPTS: Record<string, { content: string; sha: string }>

try {
  SCRIPTS = ['modify', 'fetch', 'id', 'update-schema'].reduce(
    (obj, scriptName) => {
      let distPath = pathJoin(__dirname, '..', '..', '..')
      if (!distPath.endsWith('dist')) {
        distPath = pathJoin(distPath, 'dist')
      }
      return Object.assign(obj, {
        [scriptName]: {
          content: readFileSync(
            pathJoin(distPath, 'lua', `${scriptName}.lua`),
            'utf8'
          )
        }
      })
    },
    {}
  )
} catch (e) {
  console.error(`Failed to read modify.lua ${e.stack}`)
  process.exit(1)
}

export function loadScripts(connection: Connection, cb?: () => void): void {
  for (const scriptName in SCRIPTS) {
    // which id here :/ ?
    connection.addCommand({
      command: 'SCRIPT',
      args: ['LOAD', SCRIPTS[scriptName].content],
      resolve: sha => {
        SCRIPTS[scriptName].sha = sha
        if (cb) {
          cb()
        }
      }
    })
  }
}

export function getScriptSha(scriptName: string): string | undefined {
  return SCRIPTS[scriptName].sha
}
