#pragma once

/**
 * Selva protocol frame header.
 */
struct selva_proto_header {
    int8_t cmd; /*!< Command identifier. */
    /**
     * Selva protocol frame header flags.
     */
    enum {
        SELVA_PROTO_HDR_FREQ_RES = 0x80, /*!< req = 0; res = 1 */
        SELVA_PROTO_HDR_FLAST    = 0x40, /*!< This is the last frame of the sequence. */
        SELVA_PROTO_HDR_FDEFLATE = 0x01, /*!< Compressed with deflate. */
    } __attribute__((packed)) flags;
    uint16_t seqno; /*!< Sequence number selected by the client. */
    uint32_t bsize; /*!< Size of the frame (incl the header). */
} __attribute__((packed,aligned(__alignof__(uint64_t))));

enum selva_proto_data_type {
    SELVA_PROTO_NULL = 0, /*!< A null. */
    SELVA_PROTO_ERROR = 1, /*!< An error message. */
    SELVA_PROTO_DOUBLE = 2, /*!< A double value. */
    SELVA_PROTO_LONGLONG = 3, /*!< A 64-bit integer value. */
    SELVA_PROTO_STRING = 4, /*!< A string or binary blob. */
    SELVA_PROTO_ARRAY = 5, /*!< Begin an array. */
    SELVA_PROTO_ARRAY_END = 5, /*!< Terminates an array of unknown length. */
} __attribute__((packed));

/**
 * Selva protocol error.
 */
struct selva_proto_error {
    enum selva_proto_data_type type;
    int err_code;
    char msg[0];
};

/**
 * Selva protocol double.
 */
struct selva_proto_double {
    enum selva_proto_data_type type;
    uint8_t _spare[7];
    double v;
};

/**
 * Selva protocol long long.
 */
struct selva_proto_longlong {
    enum selva_proto_data_type type;
    uint8_t _spare[7];
    uint64_t v;
};

/**
 * Selva protocol string.
 */
struct selva_proto_string {
    enum selva_proto_data_type type;
    enum {
        SELVA_PROTO_STRING_FBINARY = 0x01, /*!< Expect binary data. */
        SElVA_PROTO_STRING_FDEFLATE = 0x02, /*!< Compressed with deflate. */
    } __attribute__((packed)) flags;
    uint8_t _spare[2];
    uint32_t bsize;
    char str[0];
};

/**
 * Selva protocol array.
 */
struct selva_proto_array {
    enum selva_proto_data_type type;
    enum {
        SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH = 0x80, /*!< Start an array of unknown length and terminate it with a special token. */
    } __attribute__((packed)) flags;
    uint8_t _spare[2];
    uint32_t length;
};

static_assert(sizeof(struct selva_proto_header) == sizeof(uint64_t));
static_assert(__alignof__(struct selva_proto_header) == __alignof__(uint64_t));
static_assert(sizeof(enum selva_proto_data_type) == 1);
static_assert(sizeof_field(struct selva_proto_string, flags) == 1);
