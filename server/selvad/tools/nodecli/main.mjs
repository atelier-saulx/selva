// vim: tabstop=2 shiftwidth=2 expandtab
import { compile, deserialize } from 'data-record';
import crc32c from './crc32c.mjs';
import { connect, processMessage } from './client.mjs';
import { toSelvaProtoStrings, modify, objectGet } from './commands.mjs';

const HOST = '127.0.0.1';
const PORT = 3000;

const SelvaSubscriptions_PubsubMessage_def = compile([
  { name: 'event_type', type: 'uint8' },
  { name: 'sub_id', type: 'uint8[32]' },
  { name: 'node_id', type: 'cstring', size: 16 },
], { align: false });

const EventTypeMap = [
  'NONE',
  'SELVA_SUB_UPDATE',
  'SELVA_SUB_TRIGGER'
];

connect(PORT, HOST).then(async (client) => {
  console.log(await client.ping());
  //console.log(await client.lscmd());
  console.log(await client.cmd.ping());
  console.log(await client.cmd.ping());

  await client.cmd.flush();

  console.log(await client.cmd.modify(modify('ma00000000000001', [['field', 'haha'], ['num', 13]])));
  console.log(await client.cmd.objectGet(objectGet('', 'ma00000000000001')));
  console.log(await client.cmd.objectGet(objectGet('', 'ma00000000000001', 'field')));

  // Subscribe to the pubsub channel 0
  client.subscribe(0, (msg) => {
    // NOTE: We don't necessarily know whether we have received the complete
    // message. In fact, there is no way to know that with streams as it's the
    // nature of streaming.
    // However, we can be pretty confident with ch0 as it's only sending very
    // short messages that should fit within a single frame.
    const ev = deserialize(SelvaSubscriptions_PubsubMessage_def, processMessage(msg)[0]);
    ev.event_type = EventTypeMap[ev.event_type];
    ev.sub_id = Buffer.from(ev.sub_id).toString('hex');
    console.log('event:', ev);
  });

  await client.cmd.subscriptionsAdd(toSelvaProtoStrings('2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b61', '1', 'node', 'ma00000000000001', 'fields', 'test'));
  await client.cmd.subscriptionsRefresh(toSelvaProtoStrings('2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b61'));

  console.log(await client.cmd.modify(modify('ma00000000000001', [['test', 'new field value']])));

  //await client.hrt();
  //console.log(await client.cmd.lslang());
  //console.log(await client.cmd.lsmod());
  //console.log(await client.cmd.config());
}).catch((err) => {
  console.error(err);
  process.exit(1);
});
