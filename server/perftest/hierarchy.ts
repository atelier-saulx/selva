import { performance } from 'perf_hooks';
import {promisify} from 'util';
import { generateNSPTree } from './util/gen-tree';
import gc from './util/gc';
import newRnd, {getRandomInt} from './util/rnd';
import redis from './util/redis';

const TEST_KEY = 'test';

export default async function hierarchy() {
    const findAncestors = promisify(redis['SELVA.HIERARCHY.findancestors']).bind(redis, TEST_KEY);

    await promisify(redis.del).bind(redis)(TEST_KEY);
    await generateNSPTree(redis, TEST_KEY, 2, 1, 40, 0.05);
    process.stderr.write('Taking a dump...');
    const fullDump = await promisify(redis['SELVA.HIERARCHY.dump']).bind(redis)(TEST_KEY);
    process.stderr.write('done\n');

	const results = [];

    async function test_ancestors() {
        const rnd = newRnd('totally random');
        const n = 80;
        let mean = 0;

        const start = performance.now();
        for (let i = 0; i < n; i++) {
            let id = fullDump[getRandomInt(rnd, 0, fullDump.length)][0];

            const ancestors = await findAncestors(id);
            mean += ancestors.length / n;
        }
        const end = performance.now();
        const tTotal = end - start;

        results.push(['n_ancestors mean', Math.round(mean)]);
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
