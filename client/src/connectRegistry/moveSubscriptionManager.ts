import { SelvaClient } from '..'

type Move = { [key: string]: [number, string?] }

export default (selvaClient: SelvaClient, move: Move) => {
  let needsSort = false
  for (const id in move) {
    const [to] = move[id]
    const t = selvaClient.servers.subsManagers
    const s = id.split(':')
    const host: string = s[0]
    const port: number = Number(s[1])
    for (let i = 0; i < t.length; i++) {
      const server = t[i]
      if (server.port === port && server.host === host) {
        server.index = Number(to)
        needsSort = true
        break
      }
    }
  }
  if (needsSort) {
    selvaClient.servers.subsManagers.sort((a, b) => a.index - b.index)
  }
}
