#pragma once

#include <stdlib.h>

#include "types.h"

#define RWM_NEEDED_NUM_CSI 1500
#define RWM_CURVE_FITTING_WINDOW_SIZE 10

#define SAVITSKY_GOLAY_WINDOW_SIZE 5

typedef enum __attribute__((__packed__)) {
  PREPROCESS_NONE = 0,
  PREPROCESS_SAVITSKY_GOLAY,
  PREPROCESS_KALMAN,
  PREPROCESS_RANDOM_WAYPOINT_MODEL,
  PREPROCESS_NUM_TYPE
} preprocess_type_t;

extern int pre_process_poly_curve_fitting(csi_t *csis, size_t num_csi,
                                          size_t degree);

extern int pre_process_savitsky_golay(csi_t *csis, size_t num_csi);

extern int pre_process_kalman(csi_t *csis, size_t num_csi);

extern int pre_process_random_waypoint_model(csi_t *csis, size_t num_csi);
