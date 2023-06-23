// vim: tabstop=2 shiftwidth=2 expandtab
import { compile, deserialize } from 'data-record';
import crc32c from './crc32c.mjs';
import { connect, processMessage } from './client.mjs';
import { modify, objectGet } from './commands.mjs';

const HOST = '127.0.0.1';
const PORT = 3000;

const SelvaSubscriptions_PubsubMessage_def = compile([
  { name: 'event_type', type: 'uint8' },
  { name: 'sub_id', type: 'uint8[32]' },
  { name: 'node_id', type: 'cstring', size: 16 },
], { align: false });

connect(PORT, HOST).then(async (client) => {
  console.log(await client.ping());
  //console.log(await client.lscmd());
  console.log(await client.cmd.ping());
  console.log(await client.cmd.ping());
  console.log(await client.cmd.modify(modify('ma00000000000001', [['field', 'haha'], ['num', 13]])));
  console.log(await client.cmd.objectGet(objectGet('', 'ma00000000000001')));
  console.log(await client.cmd.objectGet(objectGet('', 'ma00000000000001', 'field')));

  client.subscribe(0, (msg) => {
    // NOTE: We don't necessarily know whether we have received the complete
    // message. In fact, there is no way to know that with streams as it's the
    // nature of streaming.
    // However, we can be pretty confident with ch0 as it's only sending very
    // short messages that should fit within a single frame.
    const ev = deserialize(SelvaSubscriptions_PubsubMessage_def, processMessage(msg)[0]);
    ev.sub_id = Buffer.from(ev.sub_id).toString('hex');
    console.log(ev);
  });
}).catch((err) => {
  console.error(err);
  process.exit(1);
});
