// vim: tabstop=2 shiftwidth=2 expandtab
import { serialize } from 'data-record';
import {
    SELVA_PROTO_STRING,
    SELVA_PROTO_ARRAY,
    selva_proto_string_def,
    selva_proto_array_def,
} from './selva_proto.mjs';

const SELVA_NODE_ID_SIZE = 16;

function serializeWithOffset(def, buf, off, obj) {
  serialize(def, buf.slice(off, off + def.size), obj);
  return def.size;
}

function serializeString(buf, off, str) {
  const bsize = Buffer.byteLength(str, 'utf8');

  const wr1 = serializeWithOffset(selva_proto_string_def, buf, off, {
    type: SELVA_PROTO_STRING,
    bsize,
  });
  const wr2 = buf.write(str, off + wr1, bsize, 'utf8');
  if (wr2 != bsize) {
    throw new Error("Buffer overflow");
  }

  return wr1 + wr2;
}

export function toSelvaProtoStrings(...strings) {
  const buf = Buffer.allocUnsafe(
    strings.map((s) => selva_proto_string_def.size + Buffer.byteLength(s)).reduce((acc, cur) => acc + cur, 0)
  );
  let off = 0;

  for (const s of strings) {
    off += serializeString(buf, off, s);
  }

  return buf;
}

export function modify(nodeId, fields) {
  const head = Buffer.alloc(
    selva_proto_array_def.size +
    selva_proto_string_def.size + SELVA_NODE_ID_SIZE +
    selva_proto_string_def.size + 0 // flags
  );
  let off = 0;

  off += serializeWithOffset(selva_proto_array_def, head, off, {
    type: SELVA_PROTO_ARRAY,
    length: 2 + 3 * fields.length,
  });

  // nodeId
  off += serializeWithOffset(selva_proto_string_def, head, off, {
    type: SELVA_PROTO_STRING,
    bsize: SELVA_NODE_ID_SIZE,
  });
  head.write(nodeId, off, SELVA_NODE_ID_SIZE, 'latin1');
  off += SELVA_NODE_ID_SIZE;

  // flags
  off += serializeWithOffset(selva_proto_string_def, head, off, {
    type: SELVA_PROTO_STRING,
    bsize: 0,
  });

  const fieldsBuf = fields.map(([field, value]) => {
    if (typeof value == 'string') {
      const buf = Buffer.alloc(
        selva_proto_string_def.size + 1 + // mod type
        selva_proto_string_def.size + Buffer.byteLength(field, 'utf8') +
        selva_proto_string_def.size + Buffer.byteLength(value, 'utf8'));
      let boff = 0;

      boff += serializeString(buf, boff, '0');
      boff += serializeString(buf, boff, field);
      boff += serializeString(buf, boff, value);

      return buf;
    } else if (typeof value == 'number') {
      const sv = `${value}`;
      const buf = Buffer.alloc(
        selva_proto_string_def.size + 1 + // mod type
        selva_proto_string_def.size + Buffer.byteLength(field, 'utf8') +
        selva_proto_string_def.size + Buffer.byteLength(sv, 'utf8'));
      let boff = 0;

      boff += serializeString(buf, boff, '0');
      boff += serializeString(buf, boff, field);
      boff += serializeString(buf, boff, sv);

      return buf;
    } else {
      throw new Error();
    }
  });

  return Buffer.concat([head, ...fieldsBuf]);
}

export function objectGet(lang, nodeId, ...fields) {
  const fields_len = () => {
    if (!fields) return 0;
    return fields.reduce((acc, cur) => acc + selva_proto_string_def.size + Buffer.byteLength(cur), 0)
  };
  const msg = Buffer.allocUnsafe(
    selva_proto_string_def.size + (lang ? Buffer.byteLength(lang) : 0) + // lang
    selva_proto_string_def.size + SELVA_NODE_ID_SIZE +
    fields_len(),
  );

  let off = 0;

  // lang
  off += serializeString(msg, off, lang || '');

  // nodeId
  off += serializeWithOffset(selva_proto_string_def, msg, off, {
    type: SELVA_PROTO_STRING,
    bsize: SELVA_NODE_ID_SIZE,
  });
  msg.write(nodeId, off, SELVA_NODE_ID_SIZE, 'latin1');
  off += SELVA_NODE_ID_SIZE;

  // opt fields
  for (const field of fields) {
    off += serializeString(msg, off, field);
  }

  return msg;
}
