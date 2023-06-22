// vim: tabstop=2 shiftwidth=2 expandtab
const net = require('net');
const { compile, serialize, deserialize } = require('data-record');
const crc32c = require('./crc32c');

const SELVA_PROTO_FRAME_SIZE_MAX = 5840;
const SELVA_PROTO_MSG_SIZE_MAX = 1073741824;

const CMD_ID_PING = 0;
const CMD_ID_ECHO = 1;
const CMD_ID_LSCMD = 2;

const SELVA_PROTO_HDR_FREQ_RES = 0x80; /*!< req = 0; res = 1 */
const SELVA_PROTO_HDR_FFMASK   = 0x60; /*!< Mask to catch fragmentation status. */
const SELVA_PROTO_HDR_FFIRST   = 0x20; /*!< This is the first frame of a sequence. */
const SELVA_PROTO_HDR_FLAST    = 0x40; /*!< This is the last frame of the sequence. */
const SELVA_PROTO_HDR_STREAM   = 0x10;
const SELVA_PROTO_HDR_BATCH    = 0x08;
const SELVA_PROTO_HDR_FDEFLATE = 0x01;

const selva_proto_header_def = compile([
  { name: 'cmd', type: 'int8'  },
  { name: 'flags', type: 'uint8' },
  { name: 'seqno', type: 'uint32_le' },
  { name: 'frame_bsize', type: 'uint16_le' },
  { name: 'msg_bsize', type: 'uint32_le' },
  { name: 'chk', type: 'uint32_le' },
], { align: false });
const SELVA_PROTO_CHECK_OFFSET = 12;

const SELVA_PROTO_NULL = 0; /*!< A null. */
const SELVA_PROTO_ERROR = 1; /*!< An error message. */
const SELVA_PROTO_DOUBLE = 2; /*!< A double value. */
const SELVA_PROTO_LONGLONG = 3; /*!< A 64-bit integer value. */
const SELVA_PROTO_STRING = 4; /*!< A string or binary blob. */
const SELVA_PROTO_ARRAY = 5; /*!< Begin an array. */
const SELVA_PROTO_ARRAY_END = 6; /*!< Terminates an array of unknown length. Uses selva_proto_control. */
const SELVA_PROTO_REPLICATION_CMD = 7; /*!< A replication message. */
const SELVA_PROTO_REPLICATION_SDB = 8; /*!< A replication db dump message. */

const selva_proto_null_def = compile([
  { name: 'type', type: 'int8' }
], { align: false });

const selva_proto_error_def = compile([
  { name: 'type', type: 'int8' },
  { name: '_spare', type: 'uint8' },
  { name: 'err_code', type: 'int16_le' },
  { name: 'bsize', type: 'uint16_le' },
], { align: false });

const selva_proto_double_def = compile([
  { name: 'type', type: 'int8' },
  { name: '_spare', type: 'uint8[7]' },
  { name: 'v', type: 'double_le' },
], { align: false });

const selva_proto_longlong_def = compile([
  { name: 'type', type: 'int8' },
  { name: 'flags', type: 'uint8' },
  { name: '_spare', type: 'uint8[6]' },
  { name: 'v', type: 'uint64_le' },
], { align: false });

const SELVA_PROTO_STRING_FBINARY = 0x01; /*!< Expect binary data. */
const SElVA_PROTO_STRING_FDEFLATE = 0x02; /*!< Compressed with deflate. */

const selva_proto_string_def = compile([
  { name: 'type', type: 'int8' },
  { name: 'flags', type: 'uint8' },
  { name: '_spare', type: 'uint8[2]' },
  { name: 'bsize', type: 'uint32_le' },
], { align: false });

const SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH = 0x80; /*!< Start an array of unknown length and terminate it with a special token. */
            SELVA_PROTO_ARRAY_FLONGLONG = 0x01; /*!< A fixed size long long array follows. No encapsulation is used. */
const SELVA_PROTO_ARRAY_FDOUBLE = 0x02; /*!< A fixed size double array follows. No encapsulation is used. */

const selva_proto_array_def = compile([
  { name: 'type', type: 'int8' },
  { name: 'flags', type: 'uint8' },
  { name: '_spare', type: 'int8[2]' },
  { name: 'length', type: 'uint32_le' },
], { align: false });

const selva_proto_control_def = compile([
  { name: 'type', type: 'int8' }
], { align: false });

// -------- Client --------

const HOST = '127.0.0.1';
const PORT = 3000;
let seqno = 0;


// seqno: Buffer
const msg_buffers = {
};

// Returns [frame_hdr, frame, rest]
function find_first_frame(buf) {
  const frame_hdr = deserialize(selva_proto_header_def, buf);
  const frame = buf.slice(0, frame_hdr.frame_bsize);
  const orig_chk = frame.readInt32LE(SELVA_PROTO_CHECK_OFFSET);
  frame.writeUInt32LE(0, SELVA_PROTO_CHECK_OFFSET);
  const comp_chk = crc32c(frame, 0) | 0;

  //console.log(frame_hdr, frame.length);
  if (orig_chk != comp_chk) {
    throw new Error(`Invalid checksum: ${orig_chk} != ${comp_chk}`);
  }

  return [frame_hdr, frame, frame_hdr.frame_bsize < buf.length ? buf.slice(frame_hdr.frame_bsize) : null];
}

function parse_hdr_null(buf) {
  return [null, selva_proto_null_def.size];
}

function parse_hdr_error(buf) {
  const def = selva_proto_error_def;
  const v = deserialize(def, buf);
  const msg_end = def.size + v.bsize;
  const msg = buf.slice(def.size, msg_end).toString('utf8');

  const err = new Error(msg);
  err.err_code = v.err_code;
  delete err.stack;

  return [err, msg_end];
}

function parse_hdr_double(buf) {
  const def = selva_proto_double_def;
  const v = deserialize(def, buf);
  return [v.v, def.size];
}

function parse_hdr_longlong(buf) {
  const def = selva_proto_longlong_def;
  const v = deserialize(def, buf);
  return [v.v, def.size];
}

function parse_hdr_string(buf) {
  const def = selva_proto_string_def;
  const v = deserialize(def, buf);
  const data_end = def.size + v.bsize;
  const data = buf.slice(def.size, data_end);
  /* TODO support deflate */
  return [(v.flags & SELVA_PROTO_STRING_FBINARY) ? data : data.toString('utf8'), data_end];
}

function parse_hdr_array(buf) {
  const def = selva_proto_array_def;
  const v = deserialize(def, buf);
  /* TODO Embedded array */
  return [v, def.size];
}

function parse_hdr_array_end(buf) {
  const def = selva_proto_control_def;
  const v = deserialize(def, buf);
  /* TODO Embedded array */
  return [v, def.size];
}

function parse_hdr_replication_cmd(buf) {
  throw new Error('ENOTSUP');
}

function parse_hdr_replication_sdb(buf) {
  throw new Error('ENOTSUP');
}

function parse_value(buf) {
  const parse_hdr = [
    parse_hdr_null,
    parse_hdr_error,
    parse_hdr_double,
    parse_hdr_longlong,
    parse_hdr_string,
    parse_hdr_array,
    parse_hdr_array_end,
    parse_hdr_replication_cmd,
    parse_hdr_replication_sdb,
  ];

  const [v, vsize] = parse_hdr[buf.readUInt8(0)](buf);
  return [v, vsize < buf.length ? buf.slice(vsize) : null];
}

function _process_seq(buf, n) {
  if (buf.length == 0) {
    return;
  }

  const result = [];
  let rest = buf;
  do {
    let v;
    [v, rest] = parse_value(rest);

    if (v.type) {
      if (v.type == SELVA_PROTO_ARRAY) {
        if (v.flags & SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH) {
          return _process_seq(rest, -2);
        } else if (v.flags & SELVA_PROTO_ARRAY_FLONGLONG) {
          const a = [];
          for (let i = 0; i < v.length; i++) {
            a.push(buf.readBigInt64LE(selva_proto_array_def.size + i * 8));
          }
          result.push(a);
          rest = buf.splice(selva_proto_array_def.size + v.length * 8);
        } else if (v.flags & SELVA_PROTO_ARRAY_FDOUBLE) {
          /* TODO */
        } else { /* Read v.length values */
          const [r, new_rest] = _process_seq(rest, v.length);
          if (r.length != v.length) {
            throw new Error(`Invalid array size: ${r.length} != ${v.length}`);
          }

          result.push(r);
          rest = new_rest;
        }
      } else if (v.type == SELVA_PROTO_ARRAY_END) {
        if (n != -2) {
          throw new Error('Unexpected SELVA_PROTO_ARRAY_END');
        }
        break;
      }
    } else {
      result.push(v);
    }

    if (n > 0) {
      n--;
    }
  } while (rest && n);

  return [result, rest];
}

function process_seq(cmd, seqno, buf) {
  return _process_seq(buf, -1)[0];
}

// Send a message buffer containing selva_proto values.
// The maximum for buf is SELVA_PROTO_MSG_SIZE_MAX. This function will split
// the message into porperly sized frames. `buf` must not contain a
// `selva_proto_header`.
function sendMsg(cmd, buf) {
  const chunkSize = SELVA_PROTO_FRAME_SIZE_MAX - selva_proto_header_def.size;

  for (let i = 0; i < buf.length; i += chunkSize) {
    const chunk = buf.slice(i, i + chunkSize);
    const frame = Buffer.allocUnsafe(selva_proto_header_def.size + chunk.length);

    let flags;
    flags |= (i == 0) ? SELVA_PROTO_HDR_FFIRST : 0;
    flags |= (i + chunkSize >= buf.length) ? SELVA_PROTO_HDR_FLAST : 0;

    serialize(selva_proto_header_def, frame, {
      cmd,
      flags,
      seqno: seqno++,
      frame_bsize: frame.length,
      msg_bsize: buf.length,
      chk: 0,
    });
    chunk.copy(frame, selva_proto_header_def.size);
    frame.writeUInt32LE(crc32c(frame, 0), SELVA_PROTO_CHECK_OFFSET);

    client.write(frame);
  }
}

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

function ping() {
  const frame = Buffer.allocUnsafe(selva_proto_header_def.size);
  serialize(selva_proto_header_def, frame, {
    cmd: CMD_ID_PING,
    flags: SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST,
    seqno: seqno++,
    frame_bsize: frame.length,
    msg_bsize: 0,
    chk: 0,
  });
  frame.writeUInt32LE(crc32c(frame, 0), SELVA_PROTO_CHECK_OFFSET);

  client.write(frame);
}

function lscmd() {
  const frame = Buffer.allocUnsafe(selva_proto_header_def.size);
  serialize(selva_proto_header_def, frame, {
    cmd: CMD_ID_LSCMD,
    flags: SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST,
    seqno: seqno++,
    frame_bsize: frame.length,
    msg_bsize: 0,
		chk: 0,
  });
  frame.writeUInt32LE(crc32c(frame, 0), SELVA_PROTO_CHECK_OFFSET);

  client.write(frame);
}

function modify(nodeId, fields) {
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

  // TODO num from discovery
  sendMsg(68, Buffer.concat([head, ...fieldsBuf]));
}

const client = new net.Socket();

client.connect(PORT, HOST, () => {
  console.log('CONNECTED TO: ' + HOST + ':' + PORT);

  ping();
  lscmd(); // Discovery
  modify('ma00000000000001', [['field', 'haha'], ['num', 13]]);
});

client.on('data', (rest) => {
  do {
    let frame_hdr, frame;
    [frame_hdr, frame, rest] = find_first_frame(rest);

    if (frame_hdr.flags & SELVA_PROTO_HDR_FREQ_RES) {
      if (frame_hdr.flags & SELVA_PROTO_HDR_FFIRST) {
        if (msg_buffers[frame_hdr.seqno]) {
          throw new Error("Seq already started");
        }

        msg_buffers[frame_hdr.seqno] = frame.slice(2 * 8);
      } else {
        if (!msg_buffers[frame_hdr.seqno]) {
          throw new Error("Seq buffer missing");
        }

        msg_buffers[frame_hdr.seqno] = Buffer.concat([msg_buffers[frame_hdr.seqno], frame.slice(2 * 8)]);
      }

      if (frame_hdr.flags & SELVA_PROTO_HDR_FLAST) {
        console.log(`RESP CMD: ${frame_hdr.cmd} SEQNO: ${frame_hdr.seqno}`);
        console.log(process_seq(frame_hdr.cmd, frame_hdr.seqno, msg_buffers[frame_hdr.seqno]));
        delete msg_buffers[frame_hdr.seqno];
      }
    } else {
      throw new Error('Requests not supported by this client');
    }
  } while (rest);
});

// Add a 'close' event handler for the client socket
client.on('close', () => {
  console.log('Connection closed');
});
