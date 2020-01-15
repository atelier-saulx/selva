import modify from '../lua/src/modify/index'
let a = modify({
  $id: 'test',
  title: { nl: 'test' },
  description: { nl: 'lekker man' }
})
// @ts-ignore
return a
