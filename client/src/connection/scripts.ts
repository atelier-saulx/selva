import { join as pathJoin } from 'path'
import { readFileSync } from 'fs'
import { Connection } from '.'

export let SCRIPTS: Record<string, { content: string; sha: string }>

try {
  SCRIPTS = ['update-schema'].reduce((obj, scriptName) => {
    let distPath = pathJoin(__dirname, '..', '..')
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

export function loadScripts(connection: Connection, cb?: () => void): void {
  for (const scriptName in SCRIPTS) {
    // which id here :/ ?
    connection.command({
      command: 'SCRIPT',
      args: ['LOAD', SCRIPTS[scriptName].content],
      resolve: (sha) => {
        SCRIPTS[scriptName].sha = sha
        if (cb) {
          cb()
        }
      },
    })
  }
}

export function getScriptSha(scriptName: string): string | undefined {
  return SCRIPTS[scriptName].sha
}
