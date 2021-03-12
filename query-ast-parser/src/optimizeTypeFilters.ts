import { Fork } from './types'
import { isFork } from './'

export default function optimizeTypeFilters(f: Fork) {
  if (f.$and) {
    const toRelocate: number[] = []

    for (let i = 0; i < f.$and.length; i++) {
      const filter = f.$and[i]

      if (isFork(filter)) {
        if (filter.$or) {
          const isAllTypeFilters = filter.$or.every((x) => {
            if (isFork(x)) {
              return false
            }

            return x.$operator === '=' && x.$field === 'type'
          })

          if (isAllTypeFilters) {
            f.isNecessary = true
            return
          }
        }

        optimizeTypeFilters(filter)
      } else {
        if (filter.$operator === '=' && filter.$field === 'type') {
          filter.isNecessary = true
          toRelocate.push(i)
        }
      }
    }

    const relocatedFilters = toRelocate.map((i) => f.$and[i])
    for (const idx of toRelocate) {
      f.$and.splice(idx, 1)
    }

    f.$and.push(...relocatedFilters)
  }
}
