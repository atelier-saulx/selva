declare module string {
  function byte(this: void, str: string): number
  function char(this: void, byte: number): string
  /** @tupleReturn */
  function gsub(
    this: void,
    str: string,
    regex: string,
    replace: string
  ): [string, number]
}
