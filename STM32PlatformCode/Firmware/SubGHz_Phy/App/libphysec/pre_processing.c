#include "pre_processing.h"

#include "utils.h"

int pre_process_poly_curve_fitting(csi_t *csis, size_t num_csi, size_t degree) {
  // TODO: implement Polynomial Curve Fitting
  return 0;
}

int pre_process_savitsky_golay(csi_t *csis, size_t num_csi) {
  // same as `PHYSEC_golay_filter` in pycom code
  // except we changed types for csis and coef array.
  // This still conserves the same logic as the original code
  // because we also use absolute values for csis while
  // holding them in a signed variable.
  int16_t coef[] = {-3, 12, 17, 12, -3}; // values likely taken from
  // https://en.wikipedia.org/wiki/Savitzky%E2%80%93Golay_filter#Appendix
  float normalization;
  int rssi_tmp; // it is not normalisze so int8_t is not a valid type.

  csi_t *filtered_csis = malloc(num_csi * sizeof(csi_t));

  for (int i_rssi = 0; i_rssi < num_csi; i_rssi++) {

    normalization = 0; // not always 35 ???
    rssi_tmp = 0;

    for (int i_coef = -2; i_coef < 3; i_coef++) {
      if (i_rssi + i_coef >= 0 && i_rssi + i_coef < num_csi) {
        rssi_tmp += coef[i_coef + 2] * csis[i_rssi + i_coef];
        normalization += coef[i_coef + 2];
      }
    }

    filtered_csis[i_rssi] = (int8_t)((float)(rssi_tmp) / normalization);
  }

  memcpy((uint8_t *)csis, filtered_csis, num_csi * sizeof(csi_t));
  free(filtered_csis);

  return 0;
}

int pre_process_kalman(csi_t *csis, size_t num_csi) {
  // TODO: implement kalman C version from matlab one
  return 0;
}

int pre_process_random_waypoint_model(csi_t *csis, size_t num_csi) {

  // TODO: Retrieve Rician Distrib K factor by computing empirical CDF(csis)
  // TODO: Retrieve Rician Distrib Omega factor by computing empirical PDF(csis)
  struct density *pdf = compute_pdf(csis, num_csi);
  if (!pdf)
    return -1;

  struct density *cdf = compute_cdf(pdf);
  if (!cdf) {
    free_density(pdf);
    return -1;
  }

  // TODO: Normalize K and Omega factors

  // TODO: Performs curve fitting on K and Omega factors and calculate C and D
  // coefficients

  // TODO: Coefficients modification: SC = \frac{n}{0.2 * l} (where n is coef
  // index and l is the length of C and D sets)

  // TODO: Copy the coefficients to the csi_t array

  free_density(pdf);
  free_density(cdf);

  return 0;
}
