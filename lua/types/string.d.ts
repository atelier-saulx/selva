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
  function find(this: void, str: string, regex: string): null | number
  function sub(this: void, str: string, start: number, end: number): string
}
