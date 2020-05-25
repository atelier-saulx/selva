import { createHash } from 'crypto'

export function hash(str: string): string {
  const hashingFn = createHash('sha256')
  hashingFn.update(str)
  return hashingFn.digest('hex')
}
