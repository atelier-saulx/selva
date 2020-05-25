import { join as pathJoin } from 'path'
import { readFileSync } from 'fs'
import { Client, addCommandToQueue } from '.'

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

export function loadScripts(client: Client): void {
  for (const scriptName in SCRIPTS) {
    addCommandToQueue(client, {
      command: 'SCRIPT',
      args: ['LOAD', SCRIPTS[scriptName].content],
      resolve: sha => {
        // console.log('LOADED SCRIPT', scriptName, 'WITH', sha)
        SCRIPTS[scriptName].sha = sha
      }
    })
  }
}

export function getScriptSha(scriptName: string): string | undefined {
  return SCRIPTS[scriptName].sha
}
