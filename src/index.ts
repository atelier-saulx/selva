type TypeA = { a: boolean, b: number }
type TypeB = { a: boolean, c: string}

export function funA(obj: TypeA): TypeA {
    return Object.assign(obj, { b: obj.b + 1 })
}

export function funB(obj: TypeB): TypeB {
    return Object.assign(obj, { a: !obj.a, c: obj.c + obj.c }) 
}

console.log(`Hello world`)