function xmur3(str: string) {
    let h = 1779033703 ^ str.length;

    for(let i = 0; i < str.length; i++) {
        h = Math.imul(h ^ str.charCodeAt(i), 3432918353);
        h = h << 13 | h >>> 19;
    }

    return () => {
        h = Math.imul(h ^ h >>> 16, 2246822507);
        h = Math.imul(h ^ h >>> 13, 3266489909);
        return (h ^= h >>> 16) >>> 0;
    }
}

function xoshiro128ss(a: number, b: number, c: number, d: number) {
    return () => {
        const t = b << 9;
        let r = a * 5;

        r = (r << 7 | r >>> 25) * 9;
        c ^= a;
        d ^= b;
        b ^= c;
        a ^= d;
        c ^= t;
        d = d << 11 | d >>> 21;

        return (r >>> 0) / 4294967296;
    }
}


export default function newRnd(seedStr: string) {
    const seed = xmur3(seedStr);

    return xoshiro128ss(seed(), seed(), seed(), seed());
}

export function getRandomInt(rnd: ReturnType<typeof newRnd>, min: number, max: number) {
    min = Math.ceil(min);
    max = Math.floor(max);

    return Math.floor(rnd() * (max - min)) + min;
}
