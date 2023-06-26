// vim: tabstop=2 shiftwidth=2 expandtab
import net from 'net';
import { serialize, deserialize } from 'data-record';
import crc32c from './crc32c.mjs';
import {
  SELVA_PROTO_FRAME_SIZE_MAX,
  SELVA_PROTO_MSG_SIZE_MAX,
  CMD_ID_PING,
  CMD_ID_LSCMD,
  SELVA_PROTO_HDR_FREQ_RES,
  SELVA_PROTO_HDR_FFIRST,
  SELVA_PROTO_HDR_FLAST,
  SELVA_PROTO_HDR_STREAM,
  SELVA_PROTO_HDR_BATCH,
  SELVA_PROTO_HDR_FDEFLATE,
  selva_proto_header_def,
  SELVA_PROTO_CHECK_OFFSET,
  SELVA_PROTO_NULL,
  SELVA_PROTO_ERROR,
  SELVA_PROTO_DOUBLE,
  SELVA_PROTO_LONGLONG,
  SELVA_PROTO_STRING,
  SELVA_PROTO_ARRAY,
  SELVA_PROTO_ARRAY_END,
  selva_proto_null_def,
  selva_proto_error_def,
  selva_proto_double_def,
  selva_proto_longlong_def,
  SELVA_PROTO_STRING_FBINARY,
  SElVA_PROTO_STRING_FDEFLATE,
  selva_proto_string_def,
  SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH,
  SELVA_PROTO_ARRAY_FLONGLONG,
  SELVA_PROTO_ARRAY_FDOUBLE,
  selva_proto_array_def,
  selva_proto_control_def,
} from './selva_proto.mjs';

// Returns current seqno and selects the next free seqno
function nextSeqno(state) {
  const isInUse = (num) => !!state.streams[num];
  const seqno = state.seqno;

  while (isInUse(++state.seqno));

  return seqno;
}

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
  return [v, def.size];
}

function parse_hdr_array_end(buf) {
  const def = selva_proto_control_def;
  const v = deserialize(def, buf);
  return [v, def.size];
}

function parse_hdr_replication_cmd(buf) {
  throw new Error('ENOTSUP');
}

function parse_hdr_replication_sdb(buf) {
  throw new Error('ENOTSUP');
}

function parse_value(buf) {
  // FIXME `[CM_ID]: fun` would be better but JS doesn't have that
  const hdr_parsers = [
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

  const type = buf.readUInt8(0);
  const parser = hdr_parsers[type];
  if (!parser) {
    throw new Error(`Invalid type: ${type}`);
  }

  const [v, vsize] = parser(buf);
  return [v, vsize < buf.length ? buf.slice(vsize) : null];
}

function processSeq(buf, n) {
  if (buf.length == 0) {
    return;
  }

  const result = [];
  let rest = buf;
  do {
    let v;
    [v, rest] = parse_value(rest);

    if (v && v.type) {
      if (v.type == SELVA_PROTO_ARRAY) {
        if (v.flags & SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH) {
          return processSeq(rest, -2);
        } else if (v.flags & SELVA_PROTO_ARRAY_FLONGLONG) {
          const a = [];
          for (let i = 0; i < v.length; i++) {
            a.push(rest.readBigInt64LE(i * 8));
          }
          result.push(a);
          rest = rest.slice(v.length * 8);
        } else if (v.flags & SELVA_PROTO_ARRAY_FDOUBLE) {
          const a = [];
          for (let i = 0; i < v.length; i++) {
            a.push(rest.readDoubleLE(i * 8));
          }
          result.push(a);
          rest = rest.slice(v.length * 8);
        } else { /* Read v.length values */
          const [r, new_rest] = processSeq(rest, v.length);
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

export function processMessage(buf) {
  return processSeq(buf, -1)[0];
}

// Send a message buffer containing selva_proto values.
// The maximum for buf is SELVA_PROTO_MSG_SIZE_MAX. This function will split
// the message into porperly sized frames. `buf` must not contain a
// `selva_proto_header`.
function sendMsg(state, cmd, buf) {
  const chunkSize = SELVA_PROTO_FRAME_SIZE_MAX - selva_proto_header_def.size;

  // Some commands don't take any payload
  if (!buf || buf.length == 0) {
    const frame = Buffer.allocUnsafe(selva_proto_header_def.size);

    serialize(selva_proto_header_def, frame, {
      cmd,
      flags: SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST,
      seqno: nextSeqno(state),
      frame_bsize: frame.length,
      msg_bsize: 0,
      chk: 0,
    });
    frame.writeUInt32LE(crc32c(frame, 0), SELVA_PROTO_CHECK_OFFSET);
    state.socket.write(frame);
    return;
  }

  if (buf.length > SELVA_PROTO_FRAME_SIZE_MAX) {
    throw new Error('Message too big');
  }

  for (let i = 0; i < buf.length; i += chunkSize) {
    const chunk = buf.slice(i, i + chunkSize);
    const frame = Buffer.allocUnsafe(selva_proto_header_def.size + chunk.length);

    let flags;
    flags |= (i == 0) ? SELVA_PROTO_HDR_FFIRST : 0;
    flags |= (i + chunkSize >= buf.length) ? SELVA_PROTO_HDR_FLAST : 0;

    serialize(selva_proto_header_def, frame, {
      cmd,
      flags,
      seqno: nextSeqno(state),
      frame_bsize: frame.length,
      msg_bsize: buf.length,
      chk: 0,
    });
    chunk.copy(frame, selva_proto_header_def.size);
    frame.writeUInt32LE(crc32c(frame, 0), SELVA_PROTO_CHECK_OFFSET);

    state.socket.write(frame);
  }
}

function ping(state) {
  const frame = Buffer.allocUnsafe(selva_proto_header_def.size);
  serialize(selva_proto_header_def, frame, {
    cmd: CMD_ID_PING,
    flags: SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST,
    seqno: nextSeqno(state),
    frame_bsize: frame.length,
    msg_bsize: 0,
    chk: 0,
  });
  frame.writeUInt32LE(crc32c(frame, 0), SELVA_PROTO_CHECK_OFFSET);

  state.socket.write(frame);
}

function lscmd(state) {
  const frame = Buffer.allocUnsafe(selva_proto_header_def.size);
  serialize(selva_proto_header_def, frame, {
    cmd: CMD_ID_LSCMD,
    flags: SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST,
    seqno: nextSeqno(state),
    frame_bsize: frame.length,
    msg_bsize: 0,
		chk: 0,
  });
  frame.writeUInt32LE(crc32c(frame, 0), SELVA_PROTO_CHECK_OFFSET);

  state.socket.write(frame);
}

function subscribe(state, commands, channel, cb) {
  const msg = Buffer.allocUnsafe(selva_proto_longlong_def.size);
  const seqno = state.seqno;

  serialize(selva_proto_longlong_def, msg, {
    type: SELVA_PROTO_LONGLONG,
    v: BigInt(channel),
  });

  sendMsg(state, commands.subscribe.id, msg);

  // Note that the response stream will never contain the channel number.
  // However, we know that the seqno will remain the same so we can lock
  // into that. We can also promise to never reuse this seqno.
  state.streams[seqno] = cb;
  console.log(`Subscribed to channel ${channel} with seqno ${seqno}`);
}

function hrt(state, commands) {
  const seqno = state.seqno;

  sendMsg(state, commands.hrt.id, null);
  state.streams[seqno] = (_buf) => console.log('Received heartbeat');
}

const cmdName2prop = (str) =>
  str.replace(/([.][a-z])/g, group =>
    group
    .toUpperCase()
    .replace('\.', '')
  );

export async function connect(port, host) {
  return new Promise((resolveClient, rejectClient) => {
    const socket = new net.Socket();
    const msgBuffers = {}; // seqno: Buffer
    const pendingReqs = {}; // seqno: { cmdId, resolve }
    const streams = []; // callbacks
    const commands = {}; // Discovered commands

    socket.connect(port, host, () => {
      console.log(`CONNECTED TO: ${host}:${port}`);

      const state = {
        socket,
        seqno: 0,
        msgBuffers,
        pendingReqs,
        streams,
      };

      const reqWrap = (cmdId, cmdFn, ...args) => new Promise((resolveReq, rejectReq) => {
        pendingReqs[state.seqno] = {
          cmdId: Number(cmdId),
          resolve: resolveReq,
          reject: rejectReq
        };

        try {
          cmdFn(state, ...args);
        } catch (e) {
          rejectReq(e);
        }
      });

      // Discover all other commands
      reqWrap(CMD_ID_LSCMD, lscmd).then((res) => {
        for (const [cmdId, cmdName] of res) {
          const prop = cmdName2prop(cmdName);
          commands[prop] = (buf) => reqWrap(cmdId, sendMsg, Number(cmdId), buf);
          commands[prop].id = Number(cmdId);
        }

        resolveClient({
          _state: state,
          sendMsg: (cmdId, buf) => sendMsg(state, cmdId, buf),
          ping: () => reqWrap(CMD_ID_PING, ping),
          lscmd: () => reqWrap(CMD_ID_LSCMD, lscmd),
          subscribe: (channel, cb) => subscribe(state, commands, channel, cb),
          hrt: () => hrt(state, commands),
          cmd: commands,
        })
      }).catch(rejectClient);
    });

    socket.on('data', (rest) => {
      do {
        let frame_hdr, frame;
        [frame_hdr, frame, rest] = find_first_frame(rest);

        if (frame_hdr.flags & SELVA_PROTO_HDR_FREQ_RES) {
          if (frame_hdr.flags & SELVA_PROTO_HDR_FFIRST) {
            if (msgBuffers[frame_hdr.seqno]) {
              throw new Error("Seq already started");
            }

            msgBuffers[frame_hdr.seqno] = frame.slice(2 * 8);
          } else {
            if (!msgBuffers[frame_hdr.seqno]) {
              throw new Error("Seq buffer missing");
            }

            msgBuffers[frame_hdr.seqno] = Buffer.concat([msgBuffers[frame_hdr.seqno], frame.slice(2 * 8)]);
          }

          if (frame_hdr.flags & SELVA_PROTO_HDR_STREAM) {
            const sub = streams[frame_hdr.seqno];
            if (sub) {
              // Make a copy of the data to avoid surprises.
              const orig = frame.slice(2 * 8);
              const msg = Buffer.allocUnsafe(orig.length);
              orig.copy(msg);

              setImmediate(() => sub(msg));
            } else {
              console.error(`Stream listener not found. seqno: ${frame_hdr.seqno}`);
            }
          } else if (frame_hdr.flags & SELVA_PROTO_HDR_FLAST) {
            console.log(`RESP CMD: ${frame_hdr.cmd} SEQNO: ${frame_hdr.seqno}`);
            const res = processMessage(msgBuffers[frame_hdr.seqno]);
            delete msgBuffers[frame_hdr.seqno];
            const req = pendingReqs[frame_hdr.seqno];

            if (req && req.cmdId === frame_hdr.cmd) {
              delete pendingReqs[frame_hdr.seqno];

              // Keep processing data if there is still something incoming.
              setImmediate(() => req.resolve(res));
            } else {
              console.error(`Unexpected response! seqno: ${frame_hdr.seqno} cmd_id: ${frame_hdr.cmd}`);
              // TODO Better error handling
              if (req) {
                req.reject(new Error());
                delete pendingReqs[frame_hdr.seqno];
              }
            }
          }
        } else {
          throw new Error('Requests not supported by this client');
        }
      } while (rest);
    });

    socket.on('close', () => {
      for (const seqno in pendingReqs) {
        const req = pendingReqs[seqno];
        req.reject(new Error('Connection reset'));
      }

      console.log('Connection closed');
    });

    socket.on('error', (e) => {
      rejectClient(e);
    });
  });
}
