// vim: tabstop=2 shiftwidth=2 expandtab
import { serialize, deserialize } from 'data-record';
import crc32c from './crc32c.mjs';
import { connect, ping, lscmd } from './client.mjs';
import { modify } from './commands.mjs';

const HOST = '127.0.0.1';
const PORT = 3000;

connect(PORT, HOST).then(async (client) => {
  console.log(await client.ping());
  //console.log(await client.lscmd());
  console.log(await client.cmd.ping());
  console.log(await client.cmd.ping());
  console.log(await client.cmd.modify(modify('ma00000000000001', [['field', 'haha'], ['num', 13]])));
}).catch((err) => {
  console.error(err);
  process.exit(1);
});
