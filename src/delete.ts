import { Id } from './schema'
import { SelvaClient } from '.'

type DeleteOptions {
  $id: true,
  $hierarchy?: boolean
}

// warning when removing all parents from something (by changing children - (default) option to remove automaticly?
async function deleteItem(client: SelvaClient, options: DeleteOptions): Promise<boolean> {
  

  return true
}

export { deleteItem, DeleteOptions }
