import { GetOptions } from '~selva/get/types'
import { isArray } from '../../util'
import { Fork } from './types'
import { isFork } from './util'

function createSearchArgs(
  getOptions: GetOptions,
  qeury: string,
  fork: Fork
): string[] {
  const $list = getOptions.$list
  if (!$list) {
    return []
  }
  let lo = 0
  let hi = 99999
  if ($list.$range) {
    lo = $list.$range[0]
    hi = $list.$range[1]
  }

  const sort: string[] = []
  if ($list.$sort) {
    sort[sort.length] = 'SORTBY'
    if (!isArray($list.$sort)) {
      $list.$sort = [$list.$sort]
    }
    for (let i = 0; i < $list.$sort.length; i++) {
      let { $field, $order } = $list.$sort[i]
      if (!$order) {
        $order = 'asc'
      }
      sort[sort.length] = $field
      sort[sort.length] =
        $order === 'asc' ? 'ASC' : $order === 'desc' ? 'DESC' : $order
    }
  }
  const searchArgs: string[] = [
    qeury,
    'NOCONTENT',
    'LIMIT',
    tostring(lo),
    tostring(hi)
  ]

  for (let i = 0; i < sort.length; i++) {
    searchArgs[searchArgs.length] = sort[i]
  }

  if (fork.$and) {
    for (let i = 0; i < fork.$and.length; i++) {
      const filter = fork.$and[i]
      if (!isFork(filter) && filter.$field === 'id') {
        const v = !isArray(filter.$value) ? [filter.$value] : filter.$value
        searchArgs[searchArgs.length] = 'INKEYS'
        searchArgs[searchArgs.length] = tostring(v.length)
        for (let j = 0; j < v.length; j++) {
          searchArgs[searchArgs.length] = tostring(v[j])
        }
        break
      }
    }
  }

  return searchArgs
}

export default createSearchArgs
