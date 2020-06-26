const PADDING_LEFT = 60;
const PADDING_RIGHT = 11;

export default function printResult(name: string, value: string | number, unit: string) {
	console.log(`${name.padEnd(PADDING_LEFT)} ${`${value}`.padStart(PADDING_RIGHT)} ${unit}`);
}
