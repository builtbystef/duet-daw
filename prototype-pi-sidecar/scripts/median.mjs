// PROTOTYPE — disposable measurement code. Do not ship.
import { readFileSync } from "node:fs";

const values = readFileSync(process.argv[2], "utf8")
  .trim()
  .split("\n")
  .map((line) => JSON.parse(line).value.milliseconds)
  .sort((a, b) => a - b);

console.log(JSON.stringify({ [process.argv[3]]: values, medianMilliseconds: values[2] }));
