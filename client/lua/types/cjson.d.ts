declare module cjson {
  function encode<T>(this: void, value: T): string
  function decode<T>(this: void, value: string): T
}
