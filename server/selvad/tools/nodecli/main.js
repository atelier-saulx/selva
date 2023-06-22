// vim: tabstop=2 shiftwidth=2 expandtab
const net = require('net');
const { compile, serialize, deserialize } = require('data-record');
const crc32c = require('./crc32c');

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

const selva_proto_array_def = compile([
  { name: 'type', type: 'int8' },
  { name: 'flags', type: 'uint8' },
  { name: '_spare', type: 'int8[2]' },
  { name: 'length', type: 'uint32_le' },
], { align: false });

// -------- Client --------

const HOST = '127.0.0.1';
const PORT = 3000;
let seqno = 0;

const client = new net.Socket();
client.connect(PORT, HOST, () => {
  console.log('CONNECTED TO: ' + HOST + ':' + PORT);

  const frame = Buffer.allocUnsafe(2 * 8);
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
});

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

  console.log(frame_hdr, frame.length);
  if (orig_chk != comp_chk) {
    throw new Error(`Invalid checksum: ${orig_chk} != ${comp_chk}`);
  }

  return [frame_hdr, frame, frame_hdr.frame_bsize < buf.length ? buf.slice(frame_hdr.frame_bsize) : null];
}

function parse_hdr_null(buf) {
  return [null, 1];
}

function parse_hdr_error(buf) {
  const v = deserialize(selva_proto_error_def, buf);
  const msg_end = 6 + v.bsize;
  const msg = buf.slice(6, msg_end).toString('utf8');
  return [ {...v, msg}, msg_end];
}

function parse_hdr_double(buf) {
  throw new Error('ENOTSUP');
}

function parse_hdr_longlong(buf) {
  throw new Error('ENOTSUP');
}

function parse_hdr_string(buf) {
  const v = deserialize(selva_proto_string_def, buf);
  const data_end = 8 + v.bsize;
  const data = buf.slice(8, data_end);
  return [ {...v, data: (v.flags & SELVA_PROTO_STRING_FBINARY) ? data : data.toString('utf8')}, data_end];
}

function parse_hdr_array(buf) {
  throw new Error('ENOTSUP');
}

function parse_hdr_array_end(buf) {
  throw new Error('ENOTSUP');
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

  [v, vsize] = parse_hdr[buf.readUInt8(0)](buf);
  delete v._spare
  return [v, vsize < buf.length ? buf.slice(vsize) : null];
}

function process_seq(cmd, seqno, buf) {
  console.log(`CMD: ${cmd} SEQNO: ${seqno}`);

  if (buf.length == 0) {
    return;
  }

  let rest = buf;
  do {
    [v, rest] = parse_value(rest);
    console.log(v);
  } while (rest);
}

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
        process_seq(frame_hdr.cmd, frame_hdr.seqno, msg_buffers[frame_hdr.seqno]);
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
