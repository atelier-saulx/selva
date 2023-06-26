// vim: tabstop=2 shiftwidth=2 expandtab
import { compile } from 'data-record';

export const SELVA_PROTO_FRAME_SIZE_MAX = 5840;
export const SELVA_PROTO_MSG_SIZE_MAX = 1073741824;

export const CMD_ID_PING = 0;
export const CMD_ID_ECHO = 1;
export const CMD_ID_LSCMD = 2;

export const SELVA_PROTO_HDR_FREQ_RES = 0x80; /*!< req = 0; res = 1 */
export const SELVA_PROTO_HDR_FFMASK   = 0x60; /*!< Mask to catch fragmentation status. */
export const SELVA_PROTO_HDR_FFIRST   = 0x20; /*!< This is the first frame of a sequence. */
export const SELVA_PROTO_HDR_FLAST    = 0x40; /*!< This is the last frame of the sequence. */
export const SELVA_PROTO_HDR_STREAM   = 0x10;
export const SELVA_PROTO_HDR_BATCH    = 0x08;
export const SELVA_PROTO_HDR_FDEFLATE = 0x01;

export const selva_proto_header_def = compile([
  { name: 'cmd', type: 'int8'  },
  { name: 'flags', type: 'uint8' },
  { name: 'seqno', type: 'uint32_le' },
  { name: 'frame_bsize', type: 'uint16_le' },
  { name: 'msg_bsize', type: 'uint32_le' },
  { name: 'chk', type: 'uint32_le' },
], { align: false });
export const SELVA_PROTO_CHECK_OFFSET = 12;

export const SELVA_PROTO_NULL = 0; /*!< A null. */
export const SELVA_PROTO_ERROR = 1; /*!< An error message. */
export const SELVA_PROTO_DOUBLE = 2; /*!< A double value. */
export const SELVA_PROTO_LONGLONG = 3; /*!< A 64-bit integer value. */
export const SELVA_PROTO_STRING = 4; /*!< A string or binary blob. */
export const SELVA_PROTO_ARRAY = 5; /*!< Begin an array. */
export const SELVA_PROTO_ARRAY_END = 6; /*!< Terminates an array of unknown length. Uses selva_proto_control. */
export const SELVA_PROTO_REPLICATION_CMD = 7; /*!< A replication message. */
export const SELVA_PROTO_REPLICATION_SDB = 8; /*!< A replication db dump message. */

export const selva_proto_null_def = compile([
  { name: 'type', type: 'int8' }
], { align: false });

export const selva_proto_error_def = compile([
  { name: 'type', type: 'int8' },
  { name: '_spare', type: 'uint8' },
  { name: 'err_code', type: 'int16_le' },
  { name: 'bsize', type: 'uint16_le' },
], { align: false });

export const selva_proto_double_def = compile([
  { name: 'type', type: 'int8' },
  { name: '_spare', type: 'uint8[7]' },
  { name: 'v', type: 'double_le' },
], { align: false });

export const selva_proto_longlong_def = compile([
  { name: 'type', type: 'int8' },
  { name: 'flags', type: 'uint8' },
  { name: '_spare', type: 'uint8[6]' },
  { name: 'v', type: 'uint64_le' },
], { align: false });

export const SELVA_PROTO_STRING_FBINARY = 0x01; /*!< Expect binary data. */
export const SElVA_PROTO_STRING_FDEFLATE = 0x02; /*!< Compressed with deflate. */

export const selva_proto_string_def = compile([
  { name: 'type', type: 'int8' },
  { name: 'flags', type: 'uint8' },
  { name: '_spare', type: 'uint8[2]' },
  { name: 'bsize', type: 'uint32_le' },
], { align: false });

export const SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH = 0x80; /*!< Start an array of unknown length and terminate it with a special token. */
export const SELVA_PROTO_ARRAY_FLONGLONG = 0x01; /*!< A fixed size long long array follows. No encapsulation is used. */
export const SELVA_PROTO_ARRAY_FDOUBLE = 0x02; /*!< A fixed size double array follows. No encapsulation is used. */

export const selva_proto_array_def = compile([
  { name: 'type', type: 'int8' },
  { name: 'flags', type: 'uint8' },
  { name: '_spare', type: 'int8[2]' },
  { name: 'length', type: 'uint32_le' },
], { align: false });

export const selva_proto_control_def = compile([
  { name: 'type', type: 'int8' }
], { align: false });
