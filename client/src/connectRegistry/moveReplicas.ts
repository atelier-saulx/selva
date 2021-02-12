import { SelvaClient } from '..'

type Move = { [key: string]: [number, string?] }

export default (selvaClient: SelvaClient, move: Move) => {
  const sortNames: Set<string> = new Set()
  for (const id in move) {
    const [to, name = 'default'] = move[id]
    const replicas = selvaClient.servers.replicas[name]
    if (replicas) {
      sortNames.add(name)
      const s = id.split(':')
      const host: string = s[0]
      const port: number = Number(s[1])
      for (let i = 0; i < replicas.length; i++) {
        const server = replicas[i]
        if (server.port === port && server.host === host) {
          server.index = Number(to)
          break
        }
      }
    } else {
      console.warn(
        'Trying to move replica on client - name does not exist yet',
        name
      )
    }
  }
  sortNames.forEach((name) =>
    selvaClient.servers.replicas[name].sort((a, b) => a.index - b.index)
  )
}
