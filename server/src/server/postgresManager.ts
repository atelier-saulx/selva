import ProcessManager from './processManager'
import { spawnSync } from 'child_process'

export default class PostgresManager extends ProcessManager {
  private pgPort: number
  private name: string

  constructor({
    port,
    password,
    name,
  }: {
    port: number
    password: string
    name: string
  }) {
    const command = `docker`
    const args = [
      `run`,
      `--rm`,
      `--name`,
      name,
      `-e`,
      `POSTGRES_PASSWORD=${password}`,
      `-p`,
      `${port}:5432`,
      `postgres:14beta2-alpine3.14`,
    ]
    super(command, {
      args,
      env: {},
    })

    this.pgPort = port
    this.name = name
  }

  getPort(): number {
    return this.pgPort
  }

  destroy(signal?: NodeJS.Signals) {
    spawnSync(`docker`, [`rm`, `-f`, this.name])
    super.destroy(signal)
  }
}
