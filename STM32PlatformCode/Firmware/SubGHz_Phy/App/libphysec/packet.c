#include "packet.h"
#include "reconciliation/reconciliation.h"
#include <stdint.h>
#include <string.h>
size_t physec_packet_get_size(physec_packet_t *packet) {
  size_t payload_size = 0;
  switch (packet->type) {
  case PHYSEC_PACKET_TYPE_PROBE:
    payload_size = sizeof(physec_probe_packet_t); // holds one byte of padding
    physec_probe_packet_t *probe = (physec_probe_packet_t *)&(packet->data);
    uint8_t padding = probe->padding[0];
    if (padding != 0)
      payload_size += (padding - 1);
    break;
  case PHYSEC_PACKET_TYPE_KEYGEN: {
    physec_keygen_packet_t *payload = (physec_keygen_packet_t *)&(packet->data);
    if (payload->kg_type == PHYSEC_KEYGEN_TYPE_DATA) {
      physec_keygen_data_t *data_payload =
          (physec_keygen_data_t *)&(payload->data);
      size_t num_embedded_dropped =
          (data_payload->num_dropped > MAX_LOSSY_PER_RADIO_FRAME)
              ? MAX_LOSSY_PER_RADIO_FRAME
              : data_payload->num_dropped;
      payload_size = sizeof(physec_keygen_packet_t) +
                     PHYSEC_PACKET_KEYGEN_DATA_HEADER_SIZE +
                     num_embedded_dropped * sizeof(quant_index_t);
    } else if (payload->kg_type == PHYSEC_KEYGEN_TYPE_RETRANSMISSION_REQ) {
      payload_size = sizeof(physec_keygen_packet_t) +
                     sizeof(physec_keygen_retransmission_req_t);
    } else if (payload->kg_type == PHYSEC_KEYGEN_TYPE_ERROR ||
               payload->kg_type == PHYSEC_KEYGEN_TYPE_DONE) {
      payload_size = sizeof(physec_keygen_packet_t);
    }
    break;
  }
  case PHYSEC_PACKET_TYPE_RECONCILIATION: {
    physec_recon_packet_t *payload = (physec_recon_packet_t *)&(packet->data);
    payload_size = payload->rec_vec_size + sizeof(payload->rec_vec_size) +
                   sizeof(payload->recon_type);
    break;
  }
  case PHYSEC_PACKET_TYPE_ENCRYPTED: {
    physec_encrypted_packet_t *payload =
        (physec_encrypted_packet_t *)&(packet->data);
    payload_size = /*sizeof(payload->magic)
                             + */
        sizeof(payload->size) + sizeof(uint8_t) * payload->size;
    //+ 4; // MIC size
    break;
  }
  case PHYSEC_PACKET_TYPE_RECONCILIATION_RESULT: {
    payload_size = sizeof(physec_recon_result_packet_t);
    break;
  }
  case PHYSEC_PACKET_TYPE_RESET:
    payload_size = sizeof(physec_reset_packet_t);
    break;
  default:
    return 0;
  }
  return sizeof(physec_packet_t) + payload_size; // header + payload size
}

bool physec_check_padding_bytes(uint8_t *data, size_t size) {
  if (size == 0)
    return false;

  uint8_t padding = data[0];
  if (size != padding)
    return false;

  for (size_t i = 1; i < size; i++) {
    if (data[i] != padding)
      return false;
  }

  return true;
}

int physec_make_padding_bytes(uint8_t *data, size_t size) {
  uint8_t padding = (uint8_t)size;
  while (size) {
    data[--size] = padding;
  }

  return padding;
}

physec_packet_t *build_probe_packet(uint8_t keygen_id, uint32_t cnt, uint8_t padding, uint8_t *buf,
                                    size_t size) {
  if (size <
      sizeof(physec_packet_t) + sizeof(physec_probe_packet_t) + padding - 1)
    return NULL;

  physec_packet_t *packet = (physec_packet_t *)buf;
  packet->type = PHYSEC_PACKET_TYPE_PROBE;
  packet->keygen_id = keygen_id;
  physec_probe_packet_t *probe = (physec_probe_packet_t *)&(packet->data);
  probe->cnt = cnt;

  if (padding == 0)
    padding = 1; // always one padding byte
  physec_make_padding_bytes(probe->padding, padding);

  return packet;
}

physec_packet_t *build_keygen_data_packet(uint8_t keygen_id, uint8_t chunk_id,
                                          quant_index_t *indexes_chunk,
                                          size_t num_indexes_chunk,
                                          size_t num_indexes, uint8_t *buf,
                                          size_t size) {
  if (size < sizeof(physec_packet_t) + sizeof(physec_keygen_packet_t) +
                 PHYSEC_PACKET_KEYGEN_DATA_HEADER_SIZE +
                 num_indexes_chunk * sizeof(quant_index_t))
    return NULL;
  if (chunk_id * MAX_LOSSY_PER_RADIO_FRAME > num_indexes)
    return NULL;

  size_t num_remaining = num_indexes - chunk_id * MAX_LOSSY_PER_RADIO_FRAME;

  physec_packet_t *packet = (physec_packet_t *)buf;
  packet->type = PHYSEC_PACKET_TYPE_KEYGEN;
  packet->keygen_id = keygen_id;
  physec_keygen_packet_t *keygen = (physec_keygen_packet_t *)&(packet->data);
  keygen->kg_type = PHYSEC_KEYGEN_TYPE_DATA;
  physec_keygen_data_t *data = (physec_keygen_data_t *)&(keygen->data);
  data->dropped_chunk_id = chunk_id;
  data->num_dropped = num_remaining;
  memcpy(data->dropped, indexes_chunk,
         num_indexes_chunk * sizeof(quant_index_t));
  // physec_keygen_data_t *data = (physec_keygen_data_t*) &(keygen->data);
  // data->chunk_id = chunk_id;
  // data->num_lossy = num_remaining;
  // memcpy(data->lossy, indexes_chunk, num_indexes_chunk *
  // sizeof(quant_index_t));

  return packet;
}

physec_packet_t *build_keygen_success_packet_lossy(uint8_t keygen_id, uint8_t *buf, size_t size) {
  if (size < sizeof(physec_packet_t) + sizeof(physec_keygen_packet_t) +
                 sizeof(physec_keygen_retransmission_req_t))
    return NULL;

  physec_packet_t *packet = (physec_packet_t *)buf;
  packet->type = PHYSEC_PACKET_TYPE_KEYGEN;
  packet->keygen_id = keygen_id;
  physec_keygen_packet_t *keygen = (physec_keygen_packet_t *)&(packet->data);
  keygen->kg_type = PHYSEC_KEYGEN_TYPE_RETRANSMISSION_REQ;
  physec_keygen_retransmission_req_t *req =
      (physec_keygen_retransmission_req_t *)&(keygen->data);
  req->lost_chunks_bitmap = 0;

  return packet;
}

physec_packet_t *build_keygen_success_packet_lossless(uint8_t keygen_id, uint8_t *buf,
                                                      size_t size) {
  if (size < sizeof(physec_packet_t) + sizeof(physec_keygen_packet_t) +
                 sizeof(physec_keygen_data_t))
    return NULL;

  physec_packet_t *packet = (physec_packet_t *)buf;
  packet->type = PHYSEC_PACKET_TYPE_KEYGEN;
  packet->keygen_id = keygen_id;
  physec_keygen_packet_t *keygen = (physec_keygen_packet_t *)&(packet->data);
  keygen->kg_type = PHYSEC_KEYGEN_TYPE_DATA;
  physec_keygen_data_t *data = (physec_keygen_data_t *)&(keygen->data);
  // data->chunk_id = 0;
  // data->num_lossy = 0;
  data->dropped_chunk_id = 0;
  data->num_dropped = 0;

  return packet;
}

physec_packet_t *build_keygen_slave_done(uint8_t keygen_id, uint8_t *buf, size_t size) {
  if (size < sizeof(physec_packet_t) + sizeof(physec_keygen_packet_t))
    return NULL;
  physec_packet_t *packet = (physec_packet_t *)buf;
  packet->type = PHYSEC_PACKET_TYPE_KEYGEN;
  packet->keygen_id = keygen_id;
  physec_keygen_packet_t *keygen = (physec_keygen_packet_t *)&(packet->data);
  keygen->kg_type = PHYSEC_KEYGEN_TYPE_DONE;
  return packet;
}

physec_packet_t *
build_keygen_retransmission_req_packet(uint8_t keygen_id, lossy_chunk_bitmap_t lost_chunks_bitmap,
                                       uint8_t *buf, size_t size) {
  if (size < sizeof(physec_packet_t) + sizeof(physec_keygen_packet_t) +
                 sizeof(physec_keygen_retransmission_req_t))
    return NULL;

  physec_packet_t *packet = (physec_packet_t *)buf;
  packet->type = PHYSEC_PACKET_TYPE_KEYGEN;
  packet->keygen_id = keygen_id;
  physec_keygen_packet_t *keygen = (physec_keygen_packet_t *)&(packet->data);
  keygen->kg_type = PHYSEC_KEYGEN_TYPE_RETRANSMISSION_REQ;
  physec_keygen_retransmission_req_t *req =
      (physec_keygen_retransmission_req_t *)&(keygen->data);
  req->lost_chunks_bitmap = lost_chunks_bitmap;

  return packet;
}

physec_packet_t *build_keygen_error_packet(uint8_t keygen_id, uint8_t *buf, size_t size) {
  if (size < sizeof(physec_packet_t) + sizeof(physec_keygen_packet_t))
    return NULL;

  physec_packet_t *packet = (physec_packet_t *)buf;
  packet->type = PHYSEC_PACKET_TYPE_KEYGEN;
  packet->keygen_id = keygen_id;
  physec_keygen_packet_t *keygen = (physec_keygen_packet_t *)&(packet->data);
  keygen->kg_type = PHYSEC_KEYGEN_TYPE_ERROR;

  return packet;
}

physec_packet_t *build_recon_packet_default(uint8_t keygen_id, uint8_t *key, size_t key_size,
                                            uint8_t *buf, size_t size) {
  if (key_size != PHYSEC_PACKET_RECON_DEFAULT_KEY_SIZE)
    return NULL;
  if (size < sizeof(physec_packet_t) + sizeof(physec_recon_packet_t) +
                 PHYSEC_PACKET_RECON_DEFAULT_KEY_SIZE)
    return NULL;

  physec_packet_t *packet = (physec_packet_t *)buf;
  packet->type = PHYSEC_PACKET_TYPE_RECONCILIATION;
  packet->keygen_id = keygen_id;
  physec_recon_packet_t *recon = (physec_recon_packet_t *)&(packet->data);
  recon->rec_vec_size = PHYSEC_PACKET_RECON_DEFAULT_KEY_SIZE;
  memcpy(recon->data.key, key, PHYSEC_PACKET_RECON_DEFAULT_KEY_SIZE);

  return packet;
}

physec_packet_t *build_recon_fe_stl_packet(uint8_t keygen_id, fe_helpers_t *helpers, uint8_t *buf,
                                           uint32_t buf_size, uint32_t key_size,
                                           uint32_t sec_param,
                                           uint32_t num_helpers) {
  if (buf_size < sizeof(physec_packet_t) + sizeof(physec_recon_packet_t) +
                     PHYSEC_PACKET_RECON_FE_STL_VEC_SIZE)
    return NULL;
  physec_packet_t *packet = (physec_packet_t *)buf;
  packet->type = PHYSEC_PACKET_TYPE_RECONCILIATION;
  packet->keygen_id = keygen_id;
  physec_recon_packet_t *recon = (physec_recon_packet_t *)&(packet->data);
  recon->recon_type = RECON_FE_STL;
  recon->rec_vec_size = num_helpers * (key_size * 3 + sec_param);
  for (uint32_t i = 0; i < num_helpers; i++) {
    uint32_t offset = (key_size * 3 + sec_param) * i;
    memcpy(&recon->data.helpers[offset], helpers->ciphers[i],
           key_size + sec_param);
    memcpy(&recon->data.helpers[offset + key_size + sec_param],
           helpers->nonces[i], key_size);
    memcpy(&recon->data.helpers[offset + key_size + sec_param + key_size],
           helpers->masks[i], key_size);
  }
  return packet;
}

physec_packet_t *build_recon_result_packet(uint8_t keygen_id, uint8_t *buf, uint32_t size,
                                           bool success) {
  if (size < sizeof(physec_packet_t) + sizeof(physec_recon_result_packet_t))
    return NULL;
  physec_packet_t *packet = (physec_packet_t *)buf;
  packet->type = PHYSEC_PACKET_TYPE_RECONCILIATION_RESULT;
  packet->keygen_id = keygen_id;
  physec_recon_result_packet_t *result =
      (physec_recon_result_packet_t *)&(packet->data);
  result->success = success;
  return packet;
}

physec_packet_t *build_encrypted_packet(uint8_t keygen_id, uint8_t *encrypted_payload,
                                        size_t encrypted_payload_size,
                                        uint8_t *buf, size_t size) {
  if (size < sizeof(physec_packet_t) + sizeof(physec_encrypted_packet_t) +
                 encrypted_payload_size)
    return NULL;
  physec_packet_t *packet = (physec_packet_t *)buf;
  packet->type = PHYSEC_PACKET_TYPE_ENCRYPTED;
  packet->keygen_id = keygen_id;
  physec_encrypted_packet_t *enc = (physec_encrypted_packet_t *)&(packet->data);
  enc->size = encrypted_payload_size;
  memcpy(enc->payload, encrypted_payload, encrypted_payload_size);

  return packet;
}

physec_packet_t *build_reset_packet(uint8_t keygen_id, uint8_t *buf, size_t size, uint8_t ack) {
  if (size < sizeof(physec_packet_t))
    return NULL;

  physec_packet_t *packet = (physec_packet_t *)buf;
  packet->type = PHYSEC_PACKET_TYPE_RESET;
  packet->keygen_id = keygen_id;
  physec_reset_packet_t *reset_packet =
      (physec_reset_packet_t *)&(packet->data);
  reset_packet->ack = ack;

  return packet;
}
