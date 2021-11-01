export const DELIMITER = '\r\n'

/*
  for (const toWrite of encodeCommand(args)) {
                this.#socket.write(toWrite);
            }
*/

/*


*/

export type RedisCommandArguments = (string | Buffer)[]

export function* encodeCommand(
  args: RedisCommandArguments
): IterableIterator<string | Buffer> {
  yield `*${args.length}${DELIMITER}`

  for (const arg of args) {
    const byteLength =
      typeof arg === 'string' ? Buffer.byteLength(arg) : arg.length
    yield `$${byteLength.toString()}${DELIMITER}`
    yield arg
    yield DELIMITER
  }
}
