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

/**
 * Selva protocol message types.
 */
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
 * Selva protocol error message.
 */
struct selva_proto_error {
    /**
     * Message type.
     * Type must be SELVA_PROTO_ERROR.
     */
    enum selva_proto_data_type type;
    uint8_t _spare;
    int16_t err_code; /*!< Error code. Typically from selva_error.h. */
    uint16_t bsize; /*!< Size of msg in bytes. */
    char msg[0]; /*!< Free form error message. Typically a human-readable string. */
};

/**
 * Selva protocol double message.
 */
struct selva_proto_double {
    /**
     * Message type.
     * Type must be SELVA_PROTO_DOUBLE.
     */
    enum selva_proto_data_type type;
    uint8_t _spare[7];
    double v;
};

/**
 * Selva protocol long long message.
 */
struct selva_proto_longlong {
    /**
     * Message type.
     * Type must be SELVA_PROTO_LONGLONG.
     */
    enum selva_proto_data_type type;
    uint8_t _spare[7];
    uint64_t v;
};

/**
 * Selva protocol string message.
 */
struct selva_proto_string {
    /**
     * Message type.
     * Type must be SELVA_PROTO_STRING.
     */
    enum selva_proto_data_type type;
    enum {
        SELVA_PROTO_STRING_FBINARY = 0x01, /*!< Expect binary data. */
        SElVA_PROTO_STRING_FDEFLATE = 0x02, /*!< Compressed with deflate. */
    } __attribute__((packed)) flags; /*! String flags. */
    uint8_t _spare[2];
    uint32_t bsize; /*!< Size of str in bytes. */
    char str[0]; /*!< A string of bytes. It's not expected to be terminated with anything. */
};

/**
 * Selva protocol array message.
 */
struct selva_proto_array {
    /**
     * Message type.
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
};

/**
 * Selva protocol control message.
 */
struct selva_proto_control {
    /**
     * Message type.
     * This struct can be used for either initial type detection or to
     * send control messages.
     * When sending a control message the type must be one of:
     * - SELVA_PROTO_ARRAY_END
     */
    enum selva_proto_data_type type;
};

static_assert(sizeof(struct selva_proto_header) == sizeof(uint64_t), "Header must be 64 bits");
static_assert(__alignof__(struct selva_proto_header) == __alignof__(uint64_t), "Header must be aligned as a 64-bit integer");
static_assert(sizeof(enum selva_proto_data_type) == 1, "data_type must be an 8-bit integer");
static_assert(sizeof_field(struct selva_proto_string, flags) == 1, "string flags must be 8-bit wide");
