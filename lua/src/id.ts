import { joinString, isArray } from './util'
import { Id, Type, ExternalId, inverseTypePrefix } from '~selva/schema'
const charset = '0123456789abcdefghijklmnopqrstuvwxyz'

function numToString(x: number, radix: number = 10): string {
  if (radix === 10) {
    return tostring(x)
  }

  let rounded = Math.floor(x)
  const result = []
  let sign = ''
  if (rounded < 0) {
    sign = '-'
    rounded = -rounded
  }

  do {
    const charsetLuaIdx = (rounded % radix) + 1
    rounded = Math.floor(rounded / radix)
    table.insert(result, 1, charset.substring(charsetLuaIdx - 1, charsetLuaIdx))
  } while (rounded !== 0)

  return sign + joinString(result, '')
}

function hash(str: string): string {
  let hash: number = 5381
  let i: number = str.length
  while (i > 0) {
    hash = bit.bxor(hash * 33, string.byte(str, --i))
  }

  return numToString(bit.rshift(hash, 0), 16)
}

type IdOptions = {
  type: Type
  externalId?: ExternalId | ExternalId[]
}

export function id({ type, externalId }: IdOptions): Id {
  const prefix = inverseTypePrefix[type]

  if (!prefix) {
    throw new Error(`Type not pre-defined ${type}`)
  }

  if (externalId) {
    if (isArray(externalId)) {
      for (let i = 0; i < externalId.length; i++) {
        externalId[i] = tostring(externalId[i])
      }
    }
    return (
      prefix +
      hash(
        isArray(externalId)
          ? joinString(<string[]>externalId, ',')
          : tostring(externalId)
      )
    )
  }

  return prefix + hash('TODO') // FIXME: ugh, can we use somethnig other than uuid() v4? we need a C extension for this
}
