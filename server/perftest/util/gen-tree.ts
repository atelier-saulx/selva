import {promisify} from 'util';


function getRandomInt(min: number, max: number) {
    min = Math.ceil(min);
    max = Math.floor(max);

    return Math.floor(Math.random() * (max - min)) + min;
}

/**
 * Generate a non-cyclic single-parent tree.
 */
export async function generateNSPTree(redis: any, key: string, medianWidth: number, widthVar: number, maxDepth: number, cutProb: number) {
    const getId = promisify(redis['SELVA.id']).bind(redis);
    const add = promisify(redis['SELVA.HIERARCHY.add']).bind(redis, key);
    let nodePool = [];

    function pickRandomParents() {
        const n = getRandomInt(0, 10);
        const parents = new Set();

        for (let i = 0; i < n; i++) {
            if (nodePool.length === 0) {
                break;
            }

            const node = nodePool[getRandomInt(0, nodePool.length)];
            parents.add(node);
            if (Math.random() < 0.05) {
                const del = getRandomInt(0, nodePool.length);
                nodePool = nodePool.splice(del, del);
            }
        }

        return [...parents];
    }

    async function gen(parentNodeId: string, depth: number) {
        if (depth <= 0 || Math.random() < cutProb) {
            return;
        }

        const nextDepth = depth - 1;
        const hw = Math.round(widthVar / 2);
        const nrChildren = getRandomInt(medianWidth - hw, medianWidth + hw);

        for (let i = 0; i < nrChildren; i++) {
            const nodeId = `a${getRandomInt(0, 5)}${await getId()}`;
            //process.stdout.write(`${parentNodeId}->${nodeId} `);
    
            const parents = pickRandomParents();
            nodePool.push(nodeId);
            await add(nodeId, ...[parentNodeId, ...parents]);
            await gen(nodeId, nextDepth);
        }
    }

    await add('head');
    return gen('head', maxDepth);
}
