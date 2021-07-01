import { SelvaClient } from '../../'
import { GetResult } from '../types'
import { setNestedResult } from '../utils'
import { ExecContext, typeCast } from './'

export function buildResultFromIdFieldAndValue(
  ctx: ExecContext,
  client: SelvaClient,
  remapped: Record<string, string>,
  field: string,
  res: string[],
  defaults: Record<string, any> = {},
  lang?: string
): GetResult {
  const o: GetResult = {}
  const used = new Set()
  for (let i = 0; i < res.length; i++) {
    let [idx, f, v] = res[i]

    const newV = typeCast(v, idx, f, client.schemas[ctx.db], lang)

    if (remapped[f]) {
      f = remapped[f]
    }

    setNestedResult(o, f.slice(field.length + 1), newV)
    used.add(f)
  }

  const allDefaults = new Set(Object.keys(defaults))
  const unusedDefaults = new Set([...allDefaults].filter((x) => !used.has(x)))

  for (const f of unusedDefaults) {
    setNestedResult(o, f.slice(field.length + 1), defaults[f])
  }

  return o
}

export function makeLangArg(languages: string[] | undefined, lang?: string) {
  if (!lang) {
    return ''
  }

  languages = languages || []

  let str = lang
  for (let i = 0; i < languages.length; i++) {
    if (languages[i] === lang) {
      continue
    }

    str += `\n${languages[i]}`
  }

  return str
}
