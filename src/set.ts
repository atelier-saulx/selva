import { Text } from './types'

// how to add item in here / merge it
type FnSet = {
  $id?: string
  $version?: string
  type?: string
  title?: Text
}

// maybe make item

function set(payload: FnSet): void {
  console.log('no', this)
}

export default set
