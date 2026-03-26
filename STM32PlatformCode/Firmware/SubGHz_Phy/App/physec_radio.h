#ifndef __PHYSEC_RADIO__
#define __PHYSEC_RADIO__

/* Includes ------------------------------------------------------------------*/
#include "libphysec/packet.h"
#include "physec_config.h"
#include "radio.h"
#include "subghz_phy_app.h"
#include "sx1276.h"
#include <stddef.h>
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/
typedef enum {
  RX,
  RX_TIMEOUT,
  RX_ERROR,
  TX,
  TX_TIMEOUT,
} States_t;

// keep track of a reset
typedef enum {
  NOT_RSTING, // usual state
  WAIT_ACK,   // alice is waiting for reset ack
  WAIT_RST,   // bob is waiting for a rst
  WAIT_PROBE, // bob is waiting for a probe
} ResetStates_t;
void reset_handler(int requested_transition);

typedef enum {
  RECV_RST,
  RECV_RST_ACK,
  USER_BTN,
  RECV_FIRST_PROBE,
  TIMEOUT,
  KG_RST_REQ,
} ResetTransitions_t;

/* Exported constants --------------------------------------------------------*/
#define lora_toa(sf, bw, pl)                                                   \
  500 // FIX: change with call to lora_time_on_air when implemented

/* Exported variables --------------------------------------------------------*/
extern RadioEvents_t RadioEvents;
extern States_t State;
extern uint8_t BufferRx[MAX_APP_BUFFER_SIZE];
extern uint8_t BufferTx[MAX_APP_BUFFER_SIZE];
/* Last  Received Buffer Size*/
extern uint16_t RxBufferSize;
/* Last  Received packer Rssi*/
extern int8_t RssiValue;
/* Last  Received packer SNR (in Lora modulation)*/
extern int8_t SnrValue;
extern int32_t random_delay;
extern int32_t time_last_pp_send_all;
extern int16_t reg_rssis[MAX_SYMBOLS_PER_FRAME];
extern size_t num_reg_rssis;
extern size_t last_added_num_reg_rssis;
extern physec_config physec_conf;
extern int32_t time_last_pp_send_all;
extern int16_t reg_rssis[MAX_SYMBOLS_PER_FRAME];
extern size_t num_reg_rssis;
extern size_t last_added_num_reg_rssis;
extern RadioEvents_t RadioEvents;
extern ResetStates_t reset_state;

/* Private function prototypes -----------------------------------------------*/
/*!
 * @brief Function to be executed on Radio Tx Done event
 */
void OnTxDone(void);

/**
 * @brief Function to be executed on Radio Rx Done event
 * @param  payload ptr of buffer received
 * @param  size buffer size
 * @param  rssi
 * @param  LoraSnr_FskCfo
 */
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi,
              int8_t LoraSnr_FskCfo);

/**
 * @brief Function executed on Radio Tx Timeout event
 */
void OnTxTimeout(void);

/**
 * @brief Function executed on Radio Rx Timeout event
 */
void OnRxTimeout(void);

/**
 * @brief Function executed on Radio Rx Error event
 */
void OnRxError(void);

uint32_t lora_time_on_air(uint8_t sf, uint8_t bw, uint8_t preamble_len,
                          uint16_t payload_len, bool crc_on);

int handle_probe(physec_probe_packet_t *probe_resp, bool is_master);
void send_probe(uint32_t counter);
physec_packet_validity_e physec_validate_packet(uint8_t *packet, size_t size);

/* === Reconciliation Implementation === */
#define FE_STL_NUM_LOCKS_PER_TRY 4
// number of security bytes
#define FE_STL_SEC 4
#define FE_STL_K 64
#define FE_STL_MAX_LOCKS 1100
// mask and nounce are AES_KEY_SIZE_IN_BYTES bytes and cipher is
// AES_KEY_SIZE_IN_BYTES+FE_STL_SEC
#define FE_STL_LOCK_SIZE (AES_KEY_SIZE_IN_BYTES * 3 + 4)
#define RECON_PACKET_HEADER_SIZE (sizeof(uint32_t) + sizeof(recon_type_t))

void fe_stl_init();
void prng_init(physec_prng_t *prng, char *name, uint32_t n);
void get_random_bytes(uint8_t *buf, uint32_t buf_size, physec_prng_t *prng);
void get_random_sampling_mask(uint8_t *mask_buf, uint32_t mask_size,
                              uint32_t hot_bits, physec_prng_t *prng);
uint32_t popcnt(uint8_t *barr, uint32_t size);
void cmox_init();
void prf(uint8_t *out_buf, uint32_t out_size, uint8_t *key_buf,
         uint32_t key_size, uint8_t *nonce, uint32_t nonce_size,
         physec_prng_t *prng);

void fe_stl_init();
void fe_stl_create_and_send_locks();
bool fe_stl_reproduce_received_locks(physec_recon_packet_t *pkt);

extern uint8_t physec_key[KEY_CAPACITY_IN_BYTES];
extern uint32_t recon_num_try;
extern uint8_t recon_key[AES_KEY_SIZE_IN_BYTES];
extern physec_prng_t r_prng;
extern physec_prng_t l_prng;
extern physec_prng_t m_prng;
#endif
