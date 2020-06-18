import { performance } from 'perf_hooks';
import {promisify} from 'util';
import { generateTree } from './util/gen-tree';
import gc from './util/gc';
import newRnd, {getRandomInt} from './util/rnd';
import redis from './util/redis';

const TEST_KEY = 'test';

function getFuncName() {
    return (new Error()).stack.match(/at (\S+)/g)[1].slice(3);
}

function calcResults(results: any[], name: string,nrAncestors: number[], tTotal: number) {
    const n = nrAncestors.length;
    const mean = nrAncestors.reduce((acc, cur) => acc + cur / n, 0);
    const stdDev = Math.sqrt(nrAncestors.map((x) => (x - mean) ** 2).reduce((acc, cur) => acc + cur / (n - 1), 0));

    results.push([`${name} mean(n_ancestors)`, Math.round(mean)]);
    results.push([`${name} σ(n_ancestors)`, Math.round(stdDev)]);
    results.push([`${name} t_find`, (tTotal / n).toFixed(2), 'ms/find']);
    results.push([`${name} t_total`, tTotal.toFixed(2), 'ms']);
}

export default async function hierarchy() {
    const find = promisify(redis['SELVA.HIERARCHY.find']).bind(redis, TEST_KEY);

    // Delete an existing hierarchy and create a fresh one
    await promisify(redis.del).bind(redis)(TEST_KEY);
    await generateTree(redis, TEST_KEY, 3, 1, 30, 10, 0.45);

    process.stderr.write('Taking a dump...');
    const fullDump = (await promisify(redis['SELVA.HIERARCHY.dump']).bind(redis)(TEST_KEY))
        .map((x: string[]) => x[0]);
    process.stderr.write('done\n');

	const results = [];
    const cases =  [
        async function test_ancestors() {
            const rnd = newRnd('totally random');
            const n = 800;
            let nrAncestors = [];

            const start = performance.now();
            for (let i = 0; i < n; i++) {
                let id = fullDump[getRandomInt(rnd, 0, fullDump.length)];

                const ancestors = await find('ancestors', id);
                nrAncestors.push(ancestors.length);
            }
            const end = performance.now();
            const tTotal = end - start;

            calcResults(results, getFuncName(), nrAncestors, tTotal);
        },
        async function test_descendants() {
            const rnd = newRnd('totally random');
            const n = 800;
            let nrAncestors = [];

            const start = performance.now();
            for (let i = 0; i < n; i++) {
                let id = fullDump[getRandomInt(rnd, 0, fullDump.length)];

                const ancestors = await find('descendants', id);
                nrAncestors.push(ancestors.length);
            }
            const end = performance.now();
            const tTotal = end - start;

            calcResults(results, getFuncName(), nrAncestors, tTotal);
        }
    ];

    for (const test of cases) {
        gc();
        await test();
    }

    return results;
}
