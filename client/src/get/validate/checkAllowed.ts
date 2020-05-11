import { GetOptions } from '../types'

export default function checkAllowed(
  props: GetOptions,
  allowed: Set<string>
): true | string {
  for (const key in props) {
    if (!allowed.has(key)) {
      return key
    }
  }

  return true
}
