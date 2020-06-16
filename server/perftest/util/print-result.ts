export default function printResult(name: string, value: string | number, unit: string) {
	console.log(`${name.padEnd(40)} ${`${value}`.padStart(11)} ${unit}`);
}
