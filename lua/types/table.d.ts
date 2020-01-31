declare namespace table {
  function insert<T>(this: void, tbl: T[], idx: number, elem: T): void
  function remove<T>(this: void, tbl: T[], idx: number): void
  function sort<T>(this: void, tbl: T[], fn?: (a: T, b: T) => boolean): void
}
