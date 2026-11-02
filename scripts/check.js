import { readdirSync, readFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
let failed = false;
for (const dir of ['src', 'public', 'scripts', 'test']) {
  for (const file of readdirSync(dir).filter(x => x.endsWith('.js'))) {
    const result = spawnSync(process.execPath, ['--check', `${dir}/${file}`], { stdio: 'inherit' });
    if (result.status !== 0) failed = true;
  }
}
for (const match of readFileSync('public/instrument-prototype.html','utf8').matchAll(/<script(?: [^>]*)?>([\s\S]*?)<\/script>/g)) {
  if (!match[1].trim()) continue;
  const result=spawnSync(process.execPath,['--check','--input-type=commonjs'],{input:match[1],encoding:'utf8'});
  if (result.status!==0) {console.error(result.stderr);failed=true;}
}
if (failed) process.exitCode = 1;
else console.log('JavaScript syntax checked.');
