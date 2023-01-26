/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#define SELVA_PROTO_FRAME_SIZE_MAX 4096
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
         * See server_start_stream().
         */
        SELVA_PROTO_HDR_STREAM   = 0x10,
        SELVA_PROTO_HDR_FDEFLATE = 0x01, /*!< Compressed with deflate. TODO Not implemented. */
    } __attribute__((packed)) flags;
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
} __attribute__((packed,aligned(__alignof__(uint64_t))));

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
} __attribute__((packed));

/**
 * Selva protocol null value.
 */
struct selva_proto_null {
    /**
     * Type.
     * Type must be SELVA_PROTO_NULL.
     */
    enum selva_proto_data_type type;
} __attribute__((packed));

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
} __attribute__((packed));

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
} __attribute__((packed));

/**
 * Selva protocol long long.
 */
struct selva_proto_longlong {
    /**
     * Type.
     * Type must be SELVA_PROTO_LONGLONG.
     */
    enum selva_proto_data_type type;
    uint8_t _spare[7];
    uint64_t v; /*!< Value. */
} __attribute__((packed));

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
        SElVA_PROTO_STRING_FDEFLATE = 0x02, /*!< Compressed with deflate. TODO Not implemented. */
    } __attribute__((packed)) flags; /*! String flags. */
    uint8_t _spare[2];
    uint32_t bsize; /*!< Size of str in bytes. */
    char data[0]; /*!< A string of bytes. It's not expected to be terminated with anything. */
} __attribute__((packed));

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
    } __attribute__((packed)) flags; /*! Array flags. */
    uint8_t _spare[2];
    uint32_t length; /*!< Length of this array; number of items. */
    char data[0]; /*!< Data (if indicated by a flag). */
} __attribute__((packed));

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
} __attribute__((packed));

#define selva_proto_typeof_str(v) _Generic((v), \
        struct selva_proto_null: "null", \
        struct selva_proto_error: "error", \
        struct selva_proto_double: "double", \
        struct selva_proto_longlong: "long long", \
        struct selva_proto_string: "string", \
        struct selva_proto_array: "array", \
        struct selva_proto_control: "control")

static_assert(sizeof(struct selva_proto_header) == (2 * sizeof(uint64_t)), "Header must be 64 bits");
static_assert(__alignof__(struct selva_proto_header) == __alignof__(uint64_t), "Header must be aligned as a 64-bit integer");
static_assert(sizeof(enum selva_proto_data_type) == 1, "data_type must be an 8-bit integer");
static_assert(sizeof_field(struct selva_proto_string, flags) == 1, "string flags must be 8-bit wide");

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
 * @}
 */
