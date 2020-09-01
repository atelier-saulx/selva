import { SelvaClient } from '.'

export const wait = (t: number = 0): Promise<void> =>
  new Promise(r => setTimeout(r, t))

export const waitUntilEvent = (selvaClient: SelvaClient, event: string) => {
  return new Promise(resolve => {
    selvaClient.once(event, () => {
      if (selvaClient.server && selvaClient.server.type === 'replica') {
        console.log('ok got dat event for replica server', event)
      }

      resolve()
    })
  })
}
