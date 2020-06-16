import { PerformanceObserver } from 'perf_hooks';
import printResult from './util/print-result';

const allTests = [];

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

const obs = new PerformanceObserver((list) => {
	const entry = list.getEntries()[0];
	const duration = entry.duration.toFixed(2);

	printResult(entry.name, duration, 'ms');
});
obs.observe({ entryTypes: ['function'] });

const tests = selectTests();
for (const test of tests) {
	const name = test.name;
	console.log(name);
	console.log('='.repeat(name.length));
	test();
	console.log();
}
