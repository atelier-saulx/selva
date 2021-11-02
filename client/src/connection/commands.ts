export const DELIMITER = '\r\n'

/*
  for (const toWrite of encodeCommand(args)) {
                this.#socket.write(toWrite);
            }
*/

/*


*/

export type RedisCommandArguments = (string | number | Buffer)[]

export function* encodeCommand(
  args: RedisCommandArguments
): IterableIterator<string | Buffer> {
  yield `*${args.length}${DELIMITER}`

  for (let arg of args) {
    if (typeof arg === 'number') {
      arg = `${arg}`
    }

    const byteLength =
      typeof arg === 'string' ? Buffer.byteLength(arg) : arg.length
    yield `$${byteLength.toString()}${DELIMITER}`
    yield arg
    yield DELIMITER
  }
}

export function command(args: RedisCommandArguments): string {
  let str = ''
  str += `*${args.length}${DELIMITER}`

  for (let arg of args) {
    if (typeof arg === 'number') {
      arg = `${arg}`
    }

    const byteLength =
      typeof arg === 'string' ? Buffer.byteLength(arg) : arg.length
    str += `$${byteLength.toString()}${DELIMITER}`
    str += arg
    str += DELIMITER
  }

  return str
}
