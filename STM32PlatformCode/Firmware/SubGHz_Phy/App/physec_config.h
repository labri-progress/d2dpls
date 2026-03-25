#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "stm32l0xx_hal.h"

// Packet for configuration are designed to be malloc-free.
//
// The packet is a simple structure with a magic number, a type and a data field.
// The type of the header helps to determine the size of the data field. 
// The data field allows encapsulating another struct representing the packet payload.
// If the payload has a variable size, it embeds an additional header to
// determine its size.

#ifdef PLS_MASTER
#define IS_MASTER		true
#else
#define IS_MASTER		false
#endif



enum __attribute__((__packed__)) {
  DEFAULT_KEYGEN_ID = (uint8_t)0x17,
};

enum __attribute__((__packed__)) {
  PHYSEC_PROBE_PADDING = (uint8_t)10,
};
enum {
  PHYSEC_PROBE_DELAY = (uint16_t)250,
  PHYSEC_PROBE_DELAY_MAX = (uint16_t)60000, // 1 minute
};

#define UART_PHYSEC_CONFIG_MAGIC			0x0c02f119
#define PHYSEC_UART_HEADER_SIZE		    5	

// Parsing error (no use)
enum {
	PHYSEC_CONFIG_PARSE_ERROR = 0,
	PHYSEC_CONFIG_PARSE_HEADER,
	PHYSEC_CONFIG_PARSE_DATA,
	PHYSEC_CONFIG_PARSE_DONE
};

// Configuration Packet types
enum __attribute__((__packed__)) {
	// Configure Key Generation parameters (algos used, csi used, role)
	PHYSEC_CONFIG_KEYGEN,
	// Configure Telemetry data outputed by PHYsec board
	PHYSEC_CONFIG_TELEMETRY,
	// Configure Physical Layer parameters (modulation, power, etc)
	PHYSEC_CONFIG_RADIO,
	// Load CSI data
	PHYSEC_CONFIG_LOAD_CSIS,
	PHYSEC_CONFIG_DONE,
	PHYSEC_CONFIG_START,
	PHYSEC_CONFIG_NUM_CMD
};

enum __attribute__((__packed__)) {
	PHYSEC_PHY_MODULATION_LORA,
	// not implemented yet
	PHYSEC_PHY_MODULATION_FSK,
};

// Main config packet structure (main header)
typedef struct __attribute__((__packed__)) {
	uint32_t magic;
	uint8_t config_type;
	uint8_t data[];
} physec_config_packet_t;

typedef struct __attribute__((__packed__)) {
	uint8_t is_master;
  uint8_t keygen_id;
	uint8_t csi_type;
	uint8_t pre_process_type;
	uint8_t quant_type;
	uint8_t recon_type;
  // number of padding bytes in 
  // probe packet
  uint8_t probe_padding;
  uint16_t probe_delay;
} physec_keygen_config;

typedef struct __attribute__((__packed__)) {
	// enable telemetry logging on UART
	uint8_t enabled:1;
	// enable `printf` like output (likely debug)
	uint8_t logging_enabled:1;
	// enable keygen telemetry data (keys, csis, timings, etc...)
	uint8_t keygen_info_enabled:1;
	uint8_t rfu:5;
} physec_telemetry_config;

typedef struct __attribute__((__packed__)) {
	uint32_t num_csi;
	int16_t csis[];
} physec_load_csi;

typedef struct __attribute__((__packed__)) {
	uint8_t modulation;
	union __attribute__((__packed__)) {
		struct __attribute__((__packed__)) {
			uint8_t sf;
			uint8_t bw;
			uint8_t power;
		} lora;
		struct __attribute__((__packed__)) {
			uint8_t datarate;
			uint8_t power;
		} fsk;
	};
} physec_physical_layer_config;

typedef struct __attribute__((__packed__)) {
	physec_keygen_config keygen;
	physec_telemetry_config telemetry;
	physec_physical_layer_config physical_layer;
} physec_config;



extern size_t physec_config_get_size(physec_config_packet_t *packet);
extern bool check_quant_method(uint8_t qtype);
extern HAL_StatusTypeDef physec_config_write_eeprom(uint8_t offset_address, physec_config *config);
extern bool physec_config_read_eeprom(uint8_t offset_address, physec_config *config);
