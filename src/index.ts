import { default as RedisClient, ConnectOptions } from './redis'

export class SelvaClient {
  public redis: RedisClient

  constructor(opts: ConnectOptions | (() => Promise<ConnectOptions>)) {
    this.redis = new RedisClient(opts)
  }

  id() {}
}

export function connect(
  opts: ConnectOptions | (() => Promise<ConnectOptions>)
): SelvaClient {
  return new SelvaClient(opts)
}

// TODO: remove these, they are just for testing that the AVA typescript setup works. replace with real tests
type TypeA = { a: boolean; b: number }
type TypeB = { a: boolean; c: string }

export function funA(obj: TypeA): TypeA {
  return Object.assign(obj, { b: obj.b + 1 })
}

export function funB(obj: TypeB): TypeB {
  return Object.assign(obj, { a: !obj.a, c: obj.c + obj.c })
}

console.log(`Hello world`)
