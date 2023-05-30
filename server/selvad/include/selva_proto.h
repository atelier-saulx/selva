/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wpadded"

/**
 * Maximum frame size including selva_proto_header.
 * This should probably be a multiple of 1460 bytes
 * for optimal network utilization.
 */
#define SELVA_PROTO_FRAME_SIZE_MAX 5840
#define SELVA_PROTO_MSG_SIZE_MAX   1073741824

struct finalizer;
struct selva_string;

/**
 * Selva protocol frame header.
 * Note that this struct should be always stored as LE.
 */
struct selva_proto_header {
    int8_t cmd; /*!< Command identifier. */
    /**
     * Selva protocol frame header flags.
     */
    enum {
        SELVA_PROTO_HDR_FREQ_RES = 0x80, /*!< req = 0; res = 1 */
        /**
         * Segment fragmentation status.
         * If both are set the frame is the only frame in a message.
         * If neither is set the frame is in the middle of a message.
         * @{
         */
        SELVA_PROTO_HDR_FFMASK   = 0x60, /*!< Mask to catch fragmentation status. */
        SELVA_PROTO_HDR_FFIRST   = 0x20, /*!< This is the first frame of a sequence. */
        SELVA_PROTO_HDR_FLAST    = 0x40, /*!< This is the last frame of the sequence. */
        /**
         * @}
         */
        /**
         * Part of a stream response.
         * This is set for both the stack response_out to indicate that a stream
         * response was created and in the stream_resp struct so that it's send
         * to the client to help parsing the stream properly.
         * See selva_start_stream().
         */
        SELVA_PROTO_HDR_STREAM   = 0x10,
        /**
         * Keep batching responses as long as this flag is set.
         * Batch processing works independently from the current frame, cmd, and
         * seqno. Therefore, batching will happen even if any of these changes,
         * as long as the connection is not interrupted.
         */
        SELVA_PROTO_HDR_BATCH    = 0x08,
        SELVA_PROTO_HDR_FDEFLATE = 0x01, /*!< Compressed with deflate. TODO Not implemented. */
    } __packed flags;
    /**
     * Sequence number selected by the request sender (typically client).
     * The sequence number doesn't change within a message (between frames) and
     * also the response uses the same sequence number. This is fine as we know
     * that frames will be delivered in the original order.
     */
    uint32_t seqno;
    uint16_t frame_bsize; /*!< Size of the frame (incl the header). */
    /**
     * The full message size excluding headers if known; Otherwise 0.
     * This can be used to help allocating sufficient buffer space for the
     * incoming message.
     */
    uint32_t msg_bsize;
    /**
     * Checksum/CRC.
     * The checksum is calculated over the whole frame with all header fields
     * set to their final values and this field zeroed.
     */
    uint32_t chk;
} __packed;
static_assert(sizeof(struct selva_proto_header) == (2 * sizeof(uint64_t)), "Header must be 64 bits");

/**
 * Selva protocol data types.
 */
enum selva_proto_data_type {
    SELVA_PROTO_NULL = 0, /*!< A null. */
    SELVA_PROTO_ERROR = 1, /*!< An error message. */
    SELVA_PROTO_DOUBLE = 2, /*!< A double value. */
    SELVA_PROTO_LONGLONG = 3, /*!< A 64-bit integer value. */
    SELVA_PROTO_STRING = 4, /*!< A string or binary blob. */
    SELVA_PROTO_ARRAY = 5, /*!< Begin an array. */
    SELVA_PROTO_ARRAY_END = 6, /*!< Terminates an array of unknown length. Uses selva_proto_control. */
    SELVA_PROTO_REPLICATION_CMD = 7, /*!< A replication message. */
    SELVA_PROTO_REPLICATION_SDB = 8, /*!< A replication db dump message. */
} __packed;
static_assert(sizeof(enum selva_proto_data_type) == 1, "data_type must be an 8-bit integer");

/**
 * Selva protocol null value.
 */
struct selva_proto_null {
    /**
     * Type.
     * Type must be SELVA_PROTO_NULL.
     */
    enum selva_proto_data_type type;
} __packed;

/**
 * Selva protocol error message.
 */
struct selva_proto_error {
    /**
     * Type.
     * Type must be SELVA_PROTO_ERROR.
     */
    enum selva_proto_data_type type;
    uint8_t _spare;
    int16_t err_code; /*!< Error code. Typically from selva_error.h. */
    uint16_t bsize; /*!< Size of msg in bytes. */
    char msg[0]; /*!< Free form error message. Typically a human-readable string. */
} __packed;

/**
 * Selva protocol double value.
 */
struct selva_proto_double {
    /**
     * Type.
     * Type must be SELVA_PROTO_DOUBLE.
     */
    enum selva_proto_data_type type;
    uint8_t _spare[7];
    double v; /*!< Value. */
} __packed;

/**
 * Selva protocol long long.
 */
struct selva_proto_longlong {
    /**
     * Type.
     * Type must be SELVA_PROTO_LONGLONG.
     */
    enum selva_proto_data_type type;
    enum {
        SELVA_PROTO_LONGLONG_FMT_HEX = 0x01, /*!< Suggested printing format is hex. */
    } __packed flags;
    uint8_t _spare[6];
    /**
     * Value.
     * This can be unsigned even if we support signed values because the struct
     * is only used for storing/passing the bits and endian conversion is
     * expected to happen before using the value.
     */
    uint64_t v;
} __packed;
static_assert(sizeof(struct selva_proto_longlong) == 2 * sizeof(uint64_t), "Must be 128 bits");

/**
 * Selva protocol string.
 */
struct selva_proto_string {
    /**
     * Type.
     * Type must be SELVA_PROTO_STRING.
     */
    enum selva_proto_data_type type;
    enum {
        SELVA_PROTO_STRING_FBINARY = 0x01, /*!< Expect binary data. */
        SElVA_PROTO_STRING_FDEFLATE = 0x02, /*!< Compressed with deflate. */
    } __packed flags; /*! String flags. */
    uint8_t _spare[2];
    uint32_t bsize; /*!< Size of data in bytes. */
    char data[0]; /*!< A string of bytes. It's not expected to be terminated with anything. */
} __packed;
static_assert(sizeof(struct selva_proto_string) == sizeof(uint64_t), "Must be 64 bits");
static_assert(sizeof_field(struct selva_proto_string, flags) == 1, "string flags must be 8-bit wide");

/**
 * Selva protocol array.
 */
struct selva_proto_array {
    /**
     * Type.
     * Type must be SELVA_PROTO_ARRAY.
     */
    enum selva_proto_data_type type;
    enum {
        SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH = 0x80, /*!< Start an array of unknown length and terminate it with a special token. */
        SELVA_PROTO_ARRAY_FLONGLONG = 0x01, /*!< A fixed size long long array follows. No encapsulation is used. */
        SELVA_PROTO_ARRAY_FDOUBLE = 0x02, /*!< A fixed size double array follows. No encapsulation is used. */
    } __packed flags; /*! Array flags. */
    uint8_t _spare[2];
    uint32_t length; /*!< Length of this array; number of items. */
    char data[0]; /*!< Data (if indicated by a flag). */
} __packed;
static_assert(sizeof(struct selva_proto_array) == sizeof(uint64_t), "Must be 64 bits");

/**
 * Selva protocol replication command header.
 * The command data follows after the header until bsize.
 */
struct selva_proto_replication_cmd {
    /**
     * Type.
     * Type must be SELVA_PROTO_REPLICATION_CMD.
     */
    enum selva_proto_data_type type;
    enum {
        SElVA_PROTO_REPLICATION_CMD_FDEFLATE = 0x02, /*!< Compressed with deflate. */
    } __packed flags; /*! String flags. */
    uint8_t _spare[5];
    int8_t cmd; /*!< Command identifier. */
    uint64_t eid; /*!< Element id of this message. */
    int64_t ts; /*!< Original command timestamp. */
    uint64_t bsize; /*!< Size of data in bytes. */
    uint8_t data[0];
} __packed;
static_assert(sizeof(struct selva_proto_replication_cmd) == 4 * sizeof(uint64_t), "Replication header should be a multiple of 64-bits");

/**
 * Selva protocol replication db dump header.
 * The command data follows after the header until bsize.
 */
struct selva_proto_replication_sdb {
    /**
     * Type.
     * Type must be SELVA_PROTO_REPLICATION_SDB.
     */
    enum selva_proto_data_type type;
    enum {
        /**
         * Pseudo-dump.
         * This message is sent to let the replica know that it's in sync with
         * the sdb_eid given in this message. The dump is omitted.
         */
        SELVA_PROTO_REPLICATION_SDB_FPSEUDO = 0x01,
    } __packed flags;
    uint8_t _spare[14];
    uint64_t eid; /*!< Element id of this message. */
    uint64_t bsize; /*!< Size of the dump. */
} __packed;
static_assert(sizeof(struct selva_proto_replication_sdb) == 4 * sizeof(uint64_t), "Replication header should be a multiple of 64-bits");
static_assert(sizeof(struct selva_proto_replication_cmd) == sizeof(struct selva_proto_replication_sdb), "Must be same size to allow easier parsing");

/**
 * Selva protocol control.
 * This struct can be used for either initial type detection or to
 * send control information.
 */
struct selva_proto_control {
    /**
     * Type.
     * When sending a control values the type must be one of:
     * - SELVA_PROTO_ARRAY_END
     */
    enum selva_proto_data_type type;
} __packed;

#define selva_proto_typeof_str(v) _Generic((v), \
        struct selva_proto_null: "null", \
        struct selva_proto_error: "error", \
        struct selva_proto_double: "double", \
        struct selva_proto_longlong: "long long", \
        struct selva_proto_string: "string", \
        struct selva_proto_array: "array", \
        struct selva_proto_control: "control")

/**
 * @addtogroup selva_proto_parse
 * Selva proto parsers.
 * The following functions are implemented in libutil.
 */

/**
 * Verify chk field of a selva_proto frame.
 * @param hdr is a pointer to the frame header. It will be modified briefly but then returned to the original state.
 * @param payload is a pointer to the frame payload.
 * @param size is the size of the payload in bytes.
 */
int selva_proto_verify_frame_chk(
        struct selva_proto_header * restrict hdr,
        const void * restrict payload,
        size_t size);

/**
 * Selva_proto type code to a human readable string.
 * @param len is optional.
 */
#if __has_c_attribute(unsequenced)
[[unsequenced]]
#else
__attribute__((const))
#endif
const char *selva_proto_type_to_str(enum selva_proto_data_type type, size_t *len);

/**
 * Parse type from a selva_proto value.
 * @param i is offset to buf. i < bsize.
 */
int selva_proto_parse_vtype(const void *buf, size_t bsize, size_t i, enum selva_proto_data_type *type_out, size_t *len_out);

/**
 * Parse selva_proto_error.
 * @param i is offset to buf. i < bsize.
 */
int selva_proto_parse_error(const void *buf, size_t bsize, size_t i, int *err_out, const char **msg_str_out, size_t *msg_len_out);

/**
 * parse selva_proto_replication.
 */
int selva_proto_parse_replication_cmd(const void *buf, size_t bsize, size_t i, uint64_t *eid, int64_t *ts, int8_t *cmd_id, int *compressed, size_t *data_size);

int selva_proto_parse_replication_sdb(const void *buf, size_t bsize, size_t i, uint64_t *eid, size_t *data_size);

/**
 * Parse a selva proto buffer into selva_strings.
 * Parse and flatten a message buffer `buf` containing only strings and string
 * arrays into an array of selva_string pointers.
 * Returned strings may be removed from the finalizer `fin` individually.
 * Also the output list `out` is added to the finalizer.
 * @param buf is a message buffer supposed to contain selva proto values.
 * @param bsize is the size of buf in bytes.
 * @param[out] out is pointer to the variable that will store the array of selva_string pointers.
 * @returns If successful, returns the number of strings in out; Otherwise an error code is returned.
 */
int selva_proto_buf2strings(struct finalizer *fin, const char *buf, size_t bsize, struct selva_string ***out);

/**
 * Parse selva proto buffer in scanf-style.
 * Format specifier: `%[width][length]specifier`
 * - `{` and `}` marks an array and `,` advances the buffer index
 * - Supported specifiers: i, d, u, f, c, s, p
 * - Length specifiers: hh, h, l, ll, j, z, t, L
 * - Using width specifier with `%s` changes output from `selva_string` to `char` buffer
 * Since using `%s` with a selva_string structure pointer gives a warning it's
 * also possible achieve the same result using `%p`.
 * `width` is used with char buffers to specify the maximum number of bytes to
 * be copied.
 * If `precision` is given as `*` with `%s` or `%p` then the string is passed as
 * a pointer and the length is written to a preceeding `size_t` pointer, e.g.
 * `selva_proto_scanf(fin, buf, sz, "%.*s", &len, &s)`.
 */
int selva_proto_scanf(struct finalizer * restrict fin, const void *restrict buf, size_t szbuf, const char * restrict fmt, ...) __attribute__((format(scanf, 4, 5)));

/**
 * @}
 */

#pragma GCC diagnostic pop
