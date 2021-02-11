import test from 'ava'
import diff, { applyPatch } from '@saulx/selva-diff'
import cmsJson from './examples/cms.json'
import cmsJsonResult from './examples/cmsResult.json'

import { deepCopy } from '@saulx/utils'

test('Weird problem in diffing in sstv cms', async t => {
  // foqj71G5W
  // foqj71G5W < has to be changed
  const a = deepCopy(cmsJson)
  const b = deepCopy(cmsJsonResult)

  const patch = diff(a, b)

  // console.dir(patch, { depth: 10 })

  const x = applyPatch(a, patch)

  // console.dir(x, { depth: 10 })

  t.pass()
})
