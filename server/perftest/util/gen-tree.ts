import {promisify} from 'util';
import cliProgress from 'cli-progress';
import newRnd, {getRandomInt} from './rnd';

const MAIN_SEED = 'This is a very good seed';

function guestimate(medianWidth: number, widthVar: number, maxDepth: number, cutProb: number) {
    const rnd = newRnd(MAIN_SEED);
    let n = 0;

    function f(depth: number) {
        n++;

        if (depth <= 0 || rnd() < cutProb) {
            return;
        }

        const hw = Math.round(widthVar / 2);
        const nrChildren = getRandomInt(rnd, medianWidth - hw, medianWidth + hw);
        for (let i = 0; i < nrChildren; i++) {
            rnd();
            f(depth - 1);
        }
    }

    f(maxDepth);

    return n;
}

export const fieldValues = [
    'test',
    'long field value',
    'lalalaalalalalalalalalal',
    'abcdef',
];

/**
 * Generate a randomized single-parent tree.
 * @param medianWidth avg number of children per node
 * @param widthVar variance of the number of children
 * @param maxDepth maximum depth of a non-acyclic descendant chain
 * @param nrRandomParentsMax Maximum number of randomly selected parents
 * @param cutProb probability that a chain doesn't reach the max length
 */
export async function generateTree(redis: any, key: string, medianWidth: number, widthVar: number, maxDepth: number, nrRandomParentsMax: number, cutProb: number) {
    const rnd = newRnd(MAIN_SEED);
    const rndParents = newRnd('parents');
    const rndFieldValue = newRnd('fields');
    const bar = new cliProgress.SingleBar({}, cliProgress.Presets.shades_classic);

    if (nrRandomParentsMax - widthVar < 0) {
        throw new Error(`(nrRandomParentsMax - widthVar = ${nrRandomParentsMax} - ${widthVar}) must be greater than or equal to 0`);
    }

    const getId = promisify(redis['SELVA.id']).bind(redis);
    const add = promisify(redis['SELVA.HIERARCHY.add']).bind(redis, key);
    const hset = promisify(redis.hset).bind(redis);
    let nodePool = [];

    function pickRandomParents(curParents: string[]) {
        const n = getRandomInt(rndParents, nrRandomParentsMax - widthVar, nrRandomParentsMax);
        const parents = new Set(curParents);

        for (let i = 0; i < n; i++) {
            if (nodePool.length === 0) {
                break;
            }

            const node = nodePool[getRandomInt(rndParents, 0, nodePool.length)];
            parents.add(node);
            /* Drop a node from the pool */
            if (rndParents() < cutProb / 10) {
                const del = getRandomInt(rndParents, 0, nodePool.length);
                nodePool = nodePool.splice(del, del);
            }
        }

        return [...parents];
    }

    async function gen(parentNodeId: string, depth: number) {
        if (depth <= 0 || rnd() < cutProb) {
            return;
        }

        const nextDepth = depth - 1;
        const hw = Math.round(widthVar / 2);
        const nrChildren = getRandomInt(rnd, medianWidth - hw, medianWidth + hw);

        for (let i = 0; i < nrChildren; i++) {
            const nodeId = `${getRandomInt(rnd, 0, 9)}X${await getId()}`;
            //process.stdout.write(`${parentNodeId}->${nodeId} `);
    
            const parents = pickRandomParents([parentNodeId]);
            nodePool.push(nodeId);
            await add(nodeId, ...parents);
            await hset(nodeId, 'field', fieldValues[getRandomInt(rndFieldValue, 0, fieldValues.length)]);
            bar.increment();

            await gen(nodeId, nextDepth);
        }
    }


    process.stderr.write('Estimating the amount of work...');
    const nrNodes = guestimate(medianWidth, widthVar, maxDepth, cutProb);
    process.stderr.write('done\nCreating nodes...\n');
    bar.start(nrNodes, 1);
    await add('head');
    await gen('head', maxDepth);
    bar.stop();
}
