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
          $type: string | string[] (optional)
          $merge: boolean (optional)
          $deepMerge: boolean (optional)
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

    if (inherit.$deepMerge && typeof inherit.$deepMerge !== 'boolean') {
      err(`$deepMerge should be boolean`)
    }

    if (inherit.$deepMerge && inherit.$merge) {
      err(`You can only provide one of $merge or $deepMerge in $inherit`)
    }

    if (inherit.$type) {
      if (!Array.isArray(inherit.$type) && typeof inherit.$type !== 'string') {
        err(`Inherit by $type must target a specific type or array of types`)
      }

      const allowed = checkAllowed(
        inherit,
        new Set(['$type', '$merge', '$deepMerge'])
      )
      if (allowed !== true) {
        err(`Field or operator ${allowed} not allowed in inherit with $type`)
      }

      return
    } else if (inherit.$item) {
      const allowed = checkAllowed(
        inherit,
        new Set(['$item', '$required', '$merge', '$deepMerge'])
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
          `Field or operator ${allowed} not allowed, only $merge or $deepMerge is allowed when no $type, $name or $item specified`
        )
      }

      return
    } else if (inherit.$deepMerge !== undefined) {
      const allowed = checkAllowed(inherit, new Set(['$deepMerge']))
      if (allowed !== true) {
        err(
          `Field or operator ${allowed} not allowed, only $deepMerge or $merge is allowed when no $type, $name or $item specified`
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
