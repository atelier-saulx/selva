import { Inherit } from '../types'
import { SelvaClient } from '../..'

import checkAllowed from './checkAllowed'

export default function validateInherit(
  client: SelvaClient,
  inherit: Inherit,
  path: string
): void {
  const err = (mainMsg?: string): never => {
    if (!mainMsg) {
      mainMsg = 'Unsupported type in operator $inherit'
    }

    throw new Error(
      `${mainMsg} for ${path}.$inherit. Required type boolean or object with any of the following signatures: 
        {
          $item: string | string[] (type)
          $required: string | string[] (field name) (optional)
        } 
        or
        {
          $type: string | string[] or $name?: string | string[] but not both (optional)
          $merge: boolean (optional)
        }
    `
    )
  }

  if (typeof inherit === 'boolean') {
    return
  }

  if (typeof inherit === 'object') {
    if (inherit.$merge && typeof inherit.$merge !== 'boolean') {
      err(`$merge should be boolean`)
    }

    if (inherit.$type) {
      if (inherit.$name) {
        err('Both $type and $name are not supported')
      }

      if (!Array.isArray(inherit.$type) && typeof inherit.$type !== 'string') {
        err(`Inherit by $type must target a specific type or array of types`)
      }

      const allowed = checkAllowed(inherit, new Set(['$type', '$merge']))
      if (allowed !== true) {
        err(`Field or operator ${allowed} not allowed in inherit with $type`)
      }

      return
    } else if (inherit.$name) {
      if (!Array.isArray(inherit.$name) && typeof inherit.$name !== 'string') {
        err(
          `Inherit by $name must target a specific name (string) or array of names`
        )
      }

      const allowed = checkAllowed(inherit, new Set(['$name', '$merge']))
      if (allowed !== true) {
        err(`Field or operator ${allowed} not allowed in inherit with $name`)
      }

      return
    } else if (inherit.$item) {
      const allowed = checkAllowed(
        inherit,
        new Set(['$item', '$required', '$merge'])
      )
      if (allowed !== true) {
        err(`Field or operator ${allowed} not allowed in inherit with $type`)
      }

      if (!Array.isArray(inherit.$item) && typeof inherit.$item !== 'string') {
        err(`Inherit by $item must target a specific type or array of types`)
      }

      if (
        inherit.$required &&
        !Array.isArray(inherit.$required) &&
        typeof inherit.$required !== 'string'
      ) {
        err(
          `In inherit by $type the $required operator must be a field name or array of field names`
        )
      }

      return
    } else if (inherit.$merge !== undefined) {
      const allowed = checkAllowed(inherit, new Set(['$merge']))
      if (allowed !== true) {
        err(
          `Field or operator ${allowed} not allowed, only $merge is allowed when no $type, $name or $item specified`
        )
      }

      return
    }

    err(
      `Object for $inherit without valid operators specified ${Object.keys(
        inherit
      )}`
    )
  }

  err()
}
