import printResult from './util/print-result';
import testHierarchy from './hierarchy';
import sleep from './util/sleep';

const allTests = [testHierarchy];

function selectTests() {
	if (process.argv.length > 2) {
		const names = process.argv.slice(2);
		const selec = [];

		for (const name of names) {
			const fn = allTests.find((f) => f.name === name);
			if (!fn) {
				console.error(`Test not found: ${name}`);
				process.exit(1);
			}
			selec.push(fn);
		}

		return selec;
	}

	return allTests;
}

const tests = selectTests();
async function run() {
    for (const test of tests) {
        const name = test.name;
        console.log(name);
        console.log('='.repeat(name.length));

        const results = await test();
        for (const result of results) {
            const [sub, value, unit] = result;
            printResult(`${name}: ${sub}`, value, unit || '');
        }
    }
}

run()
.catch((err) => {
    console.error(err);
    process.exit(1);
})
.then(() => {
    sleep(100).then(() => process.exit(0));
})
