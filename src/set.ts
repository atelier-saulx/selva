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
  console.log('yesh', this.redis)
  if (!payload.$id) {
    console.log('create item')
    if (!payload.type) {
      throw new Error('Cannot create an item if type is not provided')
    }
    // externalID
    // make root on start up?
  }
}

export default set
