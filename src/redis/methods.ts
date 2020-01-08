type args = (string | number)[]

abstract class RedisMethods {
  abstract queue(
    command: string,
    args: args,
    resolve: (x: any) => void,
    reject: (x: Error) => void
  ): void

  async move(...args: args): Promise<any> {
    console.log('roaming the earth...')
  }
}

export default RedisMethods
