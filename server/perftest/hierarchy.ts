import { performance } from 'perf_hooks';
import {promisify} from 'util';
import { generateTree } from './util/gen-tree';
import gc from './util/gc';
import newRnd, {getRandomInt} from './util/rnd';
import redis from './util/redis';

const TEST_KEY = 'test';

export default async function hierarchy() {
    const findAncestors = promisify(redis['SELVA.HIERARCHY.findancestors']).bind(redis, TEST_KEY);

    // Delete an existing hierarhy and create a fresh one
    await promisify(redis.del).bind(redis)(TEST_KEY);
    await generateTree(redis, TEST_KEY, 3, 1, 40, 10, 0.45);

    process.stderr.write('Taking a dump...');
    const fullDump = await promisify(redis['SELVA.HIERARCHY.dump']).bind(redis)(TEST_KEY);
    process.stderr.write('done\n');

	const results = [];

    async function test_ancestors() {
        const rnd = newRnd('totally random');
        const n = 200;
        let nrAncestors = [];

        const start = performance.now();
        for (let i = 0; i < n; i++) {
            let id = fullDump[getRandomInt(rnd, 0, fullDump.length)][0];

            const ancestors = await findAncestors(id);
            nrAncestors.push(ancestors.length);
        }
        const end = performance.now();
        const tTotal = end - start;

        const mean = nrAncestors.reduce((acc, cur) => acc + cur / n, 0);
        const stdDev = Math.sqrt(nrAncestors.map((x) => (x - mean) ** 2).reduce((acc, cur) => acc + cur / (n - 1), 0));

        results.push(['mean(n_ancestors)', Math.round(mean)]);
        results.push(['σ(n_ancestors)', Math.round(stdDev)]);
        results.push(['t_find', (tTotal / n).toFixed(2), 'ms/find']);
        results.push(['t_total', tTotal.toFixed(2), 'ms']);
    }

	const cases = [test_ancestors];
    for (const test of cases) {
        gc();
        await test();
    }

    return results;
}
