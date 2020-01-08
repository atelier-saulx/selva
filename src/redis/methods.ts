type args = (string | number)[]

abstract class RedisMethods {
  abstract queue(
    command: string,
    args: args,
    resolve: (x: any) => void,
    reject: (x: Error) => void
  ): void

  async dbsize(): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('dbsize', [], resolve, reject)
    })
  }

  async decr(id: string): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('decr', [id], resolve, reject)
    })
  }

  async incr(id: string): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('incr', [id], resolve, reject)
    })
  }

  async decrby(id: string, amount: number): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('decrby', [id, amount], resolve, reject)
    })
  }

  async incrby(id: string, amount: number): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('incrby', [id, amount], resolve, reject)
    })
  }

  async set(id: string, value: any): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('set', [id, value], v => resolve(v === 'OK'), reject)
    })
  }

  async del(id: string): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('del', [id], v => resolve(v === 1), reject)
    })
  }

  async get(id: string): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('get', [id], resolve, reject)
    })
  }
}

export default RedisMethods
