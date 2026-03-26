#include "utils.h"
#include "types.h"

#include <math.h>

/** Array primitives **/
gen_array_remove_at(csi_t) gen_array_remove_at(quant_index_t)

    /** Sorting algorithms **/
    gen_insert_sort(csi_t) gen_insert_sort(quant_index_t) gen_quick_sort(csi_t)

        gen_sort_check(csi_t) gen_sort_check(quant_index_t)

            void shift_array_left(void *arr, size_t size, size_t at,
                                  size_t shift_by, size_t elem_size) {
  // TODO: Implements the function. Move data to new position and override
  // garbage with zeroes
}

struct density *get_density(csi_t *csis, size_t num_csi) {
  struct density *d;

  if (num_csi == 0)
    return NULL;

  csi_t tmp[num_csi];
  memcpy(tmp, csis, num_csi * sizeof(csi_t));
  csi_t_sort(tmp, num_csi);

  csi_t q0 = tmp[0];
  csi_t last_elem = q0;
  uint16_t num_bins = 0;
  for (size_t i = 0; i < num_csi; i++) {
    if (tmp[i] != last_elem) {
      num_bins++;
      last_elem = tmp[i];
    }
  }

  d = alloc_density(num_bins);
  if (!d)
    return NULL;

  d->q_0 = q0;
  last_elem = tmp[0];
  uint16_t rep_nbr = 1;
  int j = 0;
  for (int i = 1; i < num_csi; i++) {
    if (tmp[i] != last_elem) {

      d->bins[j] = tmp[i] - last_elem;
      d->values[j] = 1.0 / ((double)(d->bins[j] * num_csi)) * ((double)rep_nbr);
      j++;

      last_elem = tmp[i];
      rep_nbr = 1;
    } else {
      rep_nbr++;
    }
  }

  return d;
}

struct density *compute_pdf(csi_t *csis, size_t num_csi) {
  struct density *d;

  if (num_csi <= 1)
    return NULL;

  csi_t_quick_sort(csis, num_csi);

  csi_t q0 = csis[0];
  csi_t last_elem = q0;
  uint16_t num_bins = 0;
  for (size_t i = 1; i < num_csi; i++) {
    if (csis[i] != last_elem) {
      num_bins++;
      last_elem = csis[i];
    }
  }

  d = alloc_density(num_bins);
  if (!d)
    return NULL;

  d->q_0 = q0;
  last_elem = csis[0];
  uint16_t rep_nbr = 1;
  size_t j = 0;
  for (size_t i = 1; i < num_csi; i++) {
    if (csis[i] != last_elem) {

      d->bins[j] = last_elem;
      d->values[j] = (double)rep_nbr / num_csi;
      j++;

      last_elem = csis[i];
      rep_nbr = 1;
    } else {
      rep_nbr++;
    }
  }

  return d;
}

struct density *compute_cdf(struct density *pdf) {
  struct density *d;

  if (!pdf || pdf->bin_nbr <= 1)
    return NULL;

  d = alloc_density(pdf->bin_nbr);
  if (!d)
    return NULL;

  d->bins[0] = pdf->bins[0];
  d->values[0] = pdf->values[0];
  for (int i = 1; i < pdf->bin_nbr; i++) {
    d->bins[i] = pdf->bins[i];
    d->values[i] = pdf->values[i] + d->values[i - 1];
  }

  return d;
}

// Pycom ported code
#define PHYSEC_QUNTIFICATION_WINDOW_LEN QUANT_MBE_WINDOW_SIZE
struct density PHYSEC_quntification_get_density(csi_t *rssi_window) {

  struct density d;

  int8_t last_ele;

  // sorting
  csi_t sorted_rssi_window[PHYSEC_QUNTIFICATION_WINDOW_LEN];
  memcpy(sorted_rssi_window, rssi_window,
         PHYSEC_QUNTIFICATION_WINDOW_LEN * sizeof(int8_t));
  if (!csi_t_is_sorted(sorted_rssi_window, PHYSEC_QUNTIFICATION_WINDOW_LEN)) {
    csi_t_sort(sorted_rssi_window, PHYSEC_QUNTIFICATION_WINDOW_LEN);
  }

  // q_0
  d.q_0 = sorted_rssi_window[0];

  // bins number
  last_ele = sorted_rssi_window[0];
  d.bin_nbr = 0;
  for (int i = 1; i < PHYSEC_QUNTIFICATION_WINDOW_LEN; i++) {
    if (sorted_rssi_window[i] != last_ele) {
      d.bin_nbr++;
      last_ele = sorted_rssi_window[i];
    }
  }

  // bins & values
  d.bins = malloc(d.bin_nbr * sizeof(int8_t));
  d.values = malloc(d.bin_nbr * sizeof(double));

  last_ele = sorted_rssi_window[0];
  char rep_nbr = 1;
  int j = 0;
  for (int i = 1; i < PHYSEC_QUNTIFICATION_WINDOW_LEN; i++) {
    if (sorted_rssi_window[i] != last_ele) {

      d.bins[j] = sorted_rssi_window[i] - last_ele;
      d.values[j] = 1.0 /
                    ((double)(d.bins[j] * PHYSEC_QUNTIFICATION_WINDOW_LEN)) *
                    ((double)rep_nbr);
      j++;

      last_ele = sorted_rssi_window[i];
      rep_nbr = 1;
    } else {
      rep_nbr++;
    }
  }

  return d;
}

void PHYSEC_quntification_free_density(struct density *d) {
  free(d->bins);
  free(d->values);
}

int8_t PHYSEC_quntification_inverse_cdf(double cdf, struct density *d) {

  if (cdf < 0 || cdf > 1) {
    return -1;
  }

  int8_t q = d->q_0, q_rest;
  double integ = 0, current_integ;

  for (int i = 0; i < d->bin_nbr; i++) {
    current_integ = d->bins[i] * d->values[i];
    if (cdf < integ + current_integ) {
      q_rest = (int8_t)((cdf - integ) / d->values[i]);
      return q + q_rest;
    } else {
      q += d->bins[i];
      integ += current_integ;
    }
  }

  return q;
}

// <--- Density function estimation

int8_t PHYSEC_quntification_compute_level_nbr(struct density *d) {

  double negatif_entropy = 0;
  double proba;

  for (int i = 0; i < d->bin_nbr; i++) {
    proba = d->values[i] * d->bins[i];
    if (proba > 0) {
      negatif_entropy += proba * log2(proba);
    }
  }

  return (int8_t)pow(2.0, -negatif_entropy);
}

/*
    return value:
        level index strating from 1.
        0 : in case of error
*/
unsigned char PHYSEC_quntification_get_level(csi_t rssi,
                                             csi_t *threshold_starts,
                                             csi_t *threshold_ends,
                                             csi_t qunatification_level_nbr) {
  unsigned char level = 1;
  for (int i = 0; i < qunatification_level_nbr; i++) {
    if (rssi < threshold_starts[i])
      return 0;
    if (rssi <= threshold_ends[i])
      return level;
    level++;
  }
  return 0;
}
