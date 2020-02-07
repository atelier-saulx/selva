import { SelvaClient } from '..'
import { GetOptions } from '../get/types'
import Observable from './observable'

type ObserveOptions = {
  getLatest: boolean
}

export default async function observe(
  client: SelvaClient,
  props: GetOptions,
  opts: ObserveOptions = { getLatest: true }
): Observable<GetResult> {
  return new Observable(observe => {
    // TODO
    return () => {}
  })
}
