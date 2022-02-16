import { SchemaMutations } from '.'
import { SelvaClient } from '..'

export default async (
  client: SelvaClient,
  mutations: SchemaMutations,
  handleMutations: (old: { [field: string]: any }) => {
    [field: string]: any
  }
) => {
  console.info(mutations)
  const setObject: { [key: string]: any } = {}

  // for (const mutation)
}
