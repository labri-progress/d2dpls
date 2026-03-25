#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "libphysec.h"
#include "reconciliation/reconciliation.h"
#include "types.h"

/*
 * /!\ This API doesn't take care of endianness
 *
 * If the two communicating boards doesn't share endianness,
 * it will not work.
 *
 * We should ensure a BigEndian (or network endian) endianness
 * on these structures.
 *
 */

#define MAX_LOSSY_CHUNKS (16) // 2**4
#define MAX_LOSSY_PER_RADIO_FRAME (16)
#define PHYSEC_PACKET_KEYGEN_DATA_HEADER_SIZE                                  \
  sizeof(uint8_t) +                                                            \
      sizeof(uint16_t) //(sizeof(physec_keygen_data_t) - sizeof(quant_index_t) *
                       // MAX_LOSSY_PER_RADIO_FRAME)
#define PHYSEC_PACKET_RECON_DEFAULT_KEY_SIZE (16)
#define PHYSEC_PACKET_RECON_FE_STL_VEC_SIZE (52)

/*!
 *	\brief Quantization indexes chunk bitmap.
 *
 *	A393216llows to keep track of the quantization indexes
 *	chunks that have been received
 */
#if MAX_LOSSY_CHUNKS <= 16
typedef uint16_t lossy_chunk_bitmap_t;
#elif MAX_LOSSY_CHUNKS <= 32
typedef uint32_t lossy_chunk_bitmap_t;
#else
#error "MAX_LOSSY_CHUNKS must be less than or equal to 32"
#endif

/*!
 *	\brief PHYsec packet types
 */
typedef enum {
  PHYSEC_PACKET_TYPE_PROBE = 0x50524f42,                 // "PROB"
  PHYSEC_PACKET_TYPE_KEYGEN = 0x4b47454e,                // "KGEN"
  PHYSEC_PACKET_TYPE_RECONCILIATION = 0x5245434f,        // "RECO"
  PHYSEC_PACKET_TYPE_RECONCILIATION_RESULT = 0x52455245, // "RECO"
  PHYSEC_PACKET_TYPE_RESET = 0x4b525354,                 // "KRST"
  PHYSEC_PACKET_TYPE_ENCRYPTED = 0x454e4352              // "ENCR"
} physec_packet_type_t;

typedef enum {
  PHYSEC_UNKNOWN_PACKET,
  PHYSEC_INVALID_PACKET,
  PHYSEC_UNRELATED_PACKET,
  PHYSEC_VALID_PACKET,
} physec_packet_validity_e;

/*!
 *	\brief PHYsec Key Generation packet subtype (Post-Process)
 */
typedef enum {
  PHYSEC_KEYGEN_TYPE_DATA = 0,
  PHYSEC_KEYGEN_TYPE_RETRANSMISSION_REQ = 1,
  PHYSEC_KEYGEN_TYPE_ERROR = 2,
  PHYSEC_KEYGEN_TYPE_DONE = 3, // slave finished quant
} physec_keygen_type_t;

/*!
 *	Packet header
 */
typedef struct __attribute__((__packed__)) {
  uint32_t type;
  // uint64_t timestamp;
  uint8_t keygen_id;
  uint8_t data[];
} physec_packet_t;

typedef struct __attribute__((__packed__)) {
  uint8_t ack;
} physec_reset_packet_t;

/*!
 *	\brief PHYsec probe payload
 */
typedef struct __attribute__((__packed__)) {
  uint32_t cnt;
  // up to 255 padding bytes
  // at least 1 padding byte
  uint8_t padding[1];
} physec_probe_packet_t;

/*!
 *	\brief PHYsec keygen payload (Post-Process header)
 */
typedef struct __attribute__((__packed__)) {
  uint8_t kg_type;
  uint8_t data[];
} physec_keygen_packet_t;

/*!
 *	\brief PHYsec keygen data payload (Post-Process payload)
 *
 *	Hold lossy quantization indexes or adaptive quantization
 *	parameters.
 *
 *	Correspond to `PHYSEC_KEYGEN_TYPE_DATA` subtype
 */
typedef struct __attribute__((__packed__)) {
  union {
    // dynamic quant params
    // TODO
    uint32_t dynparams;

    // lossy points, a maximum of 256 lossy
    // points can be chunked out and sent
    // using this
    struct {
      uint8_t chunk_id : 4; // Change MAX_LOSSY_CHUNKS and
                            // PHYSEC_PACKET_KEYGEN_HEADER_SIZE
                            // if you modify the size of this field
      uint16_t num_lossy_points : 12;
      quant_index_t lossy_points[MAX_LOSSY_PER_RADIO_FRAME];
    } lossy;
#define dropped lossy.lossy_points
#define dropped_chunk_id lossy.chunk_id
#define num_dropped lossy.num_lossy_points
  };
} physec_keygen_data_t;

// typedef struct {
//   uint8_t chunk_id:4; // 2**4 == MAX_LOSSY_CHUNKS
//                       // Change MAX_LOSSY_CHUNKS and
//                       // PHYSEC_PACKET_KEYGEN_HEADER_SIZE
//                       // if you modify the size of this field
//   uint16_t num_lossy:12;
//   quant_index_t lossy[MAX_LOSSY_PER_RADIO_FRAME];
// } physec_keygen_data_t;

/*!
 *	\brief PHYsec keygen retransmission request payload (Post-Process
 *payload)
 *
 *	Request retransmission of non received indexes chunks.
 *
 *	Correspond to `PHYSEC_KEYGEN_TYPE_RETRANSMISSION_REQ` subtype
 */
typedef struct __attribute__((__packed__)) {
  lossy_chunk_bitmap_t lost_chunks_bitmap;
} physec_keygen_retransmission_req_t;

/*!
 *	\brief PHYsec Reconciliation vector
 */
typedef struct __attribute__((__packed__)) {
  uint32_t rec_vec_size;
  recon_type_t recon_type;
  union {
    uint8_t key[PHYSEC_PACKET_RECON_DEFAULT_KEY_SIZE];
    uint8_t helpers[PHYSEC_PACKET_RECON_FE_STL_VEC_SIZE];
  } data;
  // add a MIC to check with reconciliated key
} physec_recon_packet_t;

typedef struct __attribute__((__packed__)) {
  bool success;
} physec_recon_result_packet_t;

/*!
 *	Dummy PHYsec packet for testing encryption after keygen
 */
typedef struct __attribute__((__packed__)) {
  // magic number decided during keygen
  // uint32_t magic;
  // size of the payload field
  uint8_t size;      // encrypted size
  uint8_t payload[]; // encrypted
} physec_encrypted_packet_t;

extern size_t physec_packet_get_size(physec_packet_t *packet);
// pkcs#7 like padding
extern bool physec_check_padding_bytes(uint8_t *data, size_t size);
extern int physec_make_padding_bytes(uint8_t *data, size_t size);

/** Packets build wrappers **/

// All these APIs return a pointer to a packet structure which will be written
// in the buffer passed as argument. If the buffer is too small, the function
// returns NULL.
// there is no allocation performed, thus, do not free !!!

extern physec_packet_t *build_probe_packet(uint8_t keygen_id, uint32_t cnt, uint8_t padding,
                                           uint8_t *buf, size_t size);

extern physec_packet_t *build_keygen_data_packet(uint8_t keygen_id, uint8_t chunk_id,
                                                 quant_index_t *indexes_chunk,
                                                 size_t num_indexes_chunk,
                                                 size_t num_indexes,
                                                 uint8_t *buf, size_t size);

extern physec_packet_t *build_keygen_success_packet_lossy(uint8_t keygen_id, uint8_t *buf,
                                                          size_t size);
extern physec_packet_t *build_keygen_success_packet_lossless(uint8_t keygen_id, uint8_t *buf,
                                                             size_t size);
physec_packet_t *build_keygen_slave_done(uint8_t keygen_id, uint8_t *buf, size_t size);

extern physec_packet_t *
build_keygen_retransmission_req_packet(uint8_t keygen_id, lossy_chunk_bitmap_t lost_chunks_bitmap,
                                       uint8_t *buf, size_t size);

extern physec_packet_t *build_keygen_error_packet(uint8_t keygen_id, uint8_t *buf, size_t size);

extern physec_packet_t *build_recon_packet_default(uint8_t keygen_id, uint8_t *key,
                                                   size_t key_size,
                                                   uint8_t *buf, size_t size);
extern physec_packet_t *
build_recon_fe_stl_packet(uint8_t keygen_id, fe_helpers_t *helpers, uint8_t *buf,
                          uint32_t buf_size, uint32_t key_size,
                          uint32_t sec_param, uint32_t num_helpers);
extern physec_packet_t *build_recon_packet_pcs(uint8_t keygen_id, uint32_t rec_vec_size,
                                               uint8_t *cs_vec, uint8_t *buf,
                                               size_t size);
extern physec_packet_t *build_recon_result_packet(uint8_t keygen_id, uint8_t *buf, uint32_t size,
                                                  bool success);
extern physec_packet_t *build_encrypted_packet(uint8_t keygen_id, uint8_t *payload,
                                               size_t payload_size,
                                               uint8_t *buf, size_t size);

extern physec_packet_t *build_reset_packet(uint8_t keygen_id, uint8_t *buf, size_t size,
                                           uint8_t ack);
