declare module string {
  function byte(this: void, str: string, idx: number): number
  /** @tupleReturn */
  function gsub(
    this: void,
    str: string,
    regex: string,
    replace: string
  ): [string, number]
}
