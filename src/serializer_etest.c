// ================
// Serializer etest
// ================
//
// Testing serializing and deserializing messages
//
// Project includes
// ================
//
// .. code-block:: cpp
//
#include "libchirp.h"
#include "quickcheck_test.h"
#include "serializer.h"
#include "util.h"

// System includes
// ===============
//
// .. code-block:: cpp
//
#include <string.h>

// Definitions
// ===========
//
// .. code-block:: cpp
//

int
main(void)
{
    ch_libchirp_init();
    for (int i = 0; i < 1000; ++i) {
        ch_buf       buf[CH_SR_WIRE_MESSAGE_SIZE];
        ch_message_t in;
        ch_message_t out;
        memset(&in, 0, sizeof(in));
        memset(&out, 0, sizeof(out));
        ch_random_ints_as_bytes(in.identity, CH_ID_SIZE);
        in.serial     = ch_qc_pgen_uint32_t();
        in.type       = ch_qc_pgen_uint8_t();
        in.header_len = ch_qc_pgen_uint16_t();
        in.data_len   = ch_qc_pgen_uint32_t();
        ch_sr_msg_to_buf(&in, buf, in.serial);
        ch_sr_buf_to_msg(buf, &out);
        if (memcmp(&in, &out, sizeof(in)) != 0) {
            char id_str[CH_ID_SIZE * 2 + 1];
            ch_bytes_to_hex(in.identity, CH_ID_SIZE, id_str, sizeof(id_str));
            printf("Serializarion failed\nIn message\n"
                   "     identity: %s\n"
                   "       serial: %u\n"
                   "         type: %u\n"
                   "   header_len: %u\n"
                   "     data_len: %u\n",
                   id_str,
                   in.serial,
                   in.type,
                   in.header_len,
                   in.data_len);
            ch_bytes_to_hex(out.identity, CH_ID_SIZE, id_str, sizeof(id_str));
            printf("Out message\n"
                   "     identity: %s\n"
                   "       serial: %u\n"
                   "         type: %u\n"
                   "   header_len: %u\n"
                   "     data_len: %u\n",
                   id_str,
                   out.serial,
                   out.type,
                   out.header_len,
                   out.data_len);
            return 1;
        }
    }
    printf("Test successful\n");
    ch_libchirp_cleanup();
    return 0;
}
