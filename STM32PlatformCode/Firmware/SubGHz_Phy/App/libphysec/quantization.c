#include "quantization.h"
#include "types.h"
#include "utils.h"

#include "subghz_phy_app.h"
#include "utilities_conf.h"
#include "utilities_def.h"

#include <math.h>

/*!
 *  \brief	Returns the graycode of `n`
 *
 *  \param	n		the integer to translate into graycode
 *					(n cannot be greater than 255, use
 *					other graycode primitives instead)
 */
uint8_t graycode_u8(uint8_t n) { return n ^ (n >> 1); }

/******* Bitkeys manipulation functions *******/
#define bit_set(byte, n) byte = (byte & ~(1U << n)) | (1U << n)
#define bit_clear(byte, n) byte = (byte & ~(1U << n))
/*!
 *	\brief	Add a bit to a key at a specific bit index
 *
 */
void add_bit_to_key(uint8_t *key, size_t key_size, size_t at, uint8_t bit_val) {
  if (at >= key_size)
    return;
  int byte_idx = at / 8;
  int in_byte_bit_idx = /*7-*/ (at % 8);
  if (bit_val) {
    bit_set(key[byte_idx], in_byte_bit_idx);
  } else {
    bit_clear(key[byte_idx], in_byte_bit_idx);
  }
}

/*!
 *	\brief	Add a bitstring to a key at a specific bit index
 *
 *	Can be used to concatenate keys
 */
int add_bits_to_key(uint8_t *key, size_t key_capacity_in_bits, size_t at,
                    uint8_t *bitstring, size_t n_bits) {
  if (at >= key_capacity_in_bits)
    return key_capacity_in_bits;
  size_t num_max =
      (at + n_bits > key_capacity_in_bits) ? key_capacity_in_bits : n_bits + at;
  for (size_t i = at, j = 0; i < num_max; i++, j++) {
    int bit_index = j % 8;
    int byte_index = j / 8;
    uint8_t bitval = (bitstring[byte_index] >> bit_index) & 1;
    add_bit_to_key(key, key_capacity_in_bits, i, bitval);
  }
  return num_max;
}

uint8_t get_bit_at(const uint8_t *key, size_t key_size_in_bits, size_t at) {
  if (at >= key_size_in_bits)
    return 0;
  int byte_idx = at / 8;
  int in_byte_bit_idx = (at % 8);
  return (key[byte_idx] >> in_byte_bit_idx) & 1;
}

/*!
 *	\brief	Remove the bit at position 'at' in the key by shifting the bits
 * to the left
 */
size_t shift_key_bits_left(uint8_t *key, size_t key_size, size_t at) {
  size_t ks = key_size;
  if (at >= ks)
    return key_size;
  for (size_t i = at; i < ks - 1; i++) {
    int bit_index = (i + 1) % 8;
    int byte_index = (i + 1) / 8;
    uint8_t bitval = key[byte_index] >> (7 - bit_index) & 1;
    add_bit_to_key(key, ks, i, bitval);
  }
  add_bit_to_key(key, ks, ks - 1, 0);
  return key_size - 1;
}

float csi_proba(csi_t val, csi_t *csis, size_t n_csi) {
  size_t count = 0;
  for (size_t i = 0; i < n_csi; i++)
    if (csis[i] == val)
      count++;
  return (float)count / n_csi;
}

float csis_entropy(int16_t *measures, size_t n_measures) {
  float sum = 0;
  for (size_t i = 0; i < n_measures; i++) {
    float p_x = csi_proba(measures[i], measures, n_measures);
    sum += p_x * log2(p_x);
  }
  return (-sum);
}

int32_t rssi_sum(int16_t *measures, size_t n_measures) {
  int32_t sum = 0;
  for (size_t i = 0; i < n_measures; i++)
    sum += measures[i];
  return sum;
}

float rssi_mean(int16_t *measures, size_t n_measures) {
  int32_t sum = rssi_sum(measures, n_measures);
  return (float)sum / (float)n_measures;
}

float rssi_variance(int16_t *measures, size_t n_measures) {
  float mean = rssi_mean(measures, n_measures);
  float sum = 0;
  for (size_t i = 0; i < n_measures; i++)
    sum += pow((float)measures[i] - mean, 2);
  return sum / (float)n_measures;
}

int16_t i16_max(int16_t *measures, size_t n_measures) {
  int16_t max = measures[0];
  for (size_t i = 1; i < n_measures; i++)
    if (measures[i] > max)
      max = measures[i];
  return max;
}

int16_t i16_min(int16_t *measures, size_t n_measures) {
  int16_t min = measures[0];
  for (size_t i = 1; i < n_measures; i++)
    if (measures[i] < min)
      min = measures[i];
  return min;
}

// FIX: Relaxe properties of required number of csi to faster KeyGen in good
// wireless env conditions
// bool
// quant_is_enough_csi(quant_type_t qtype, size_t num_csi)
//{
//	// TODO: Regarding the quantization method, check if there is enough
// gathered csi to perform quantization 	switch (qtype) {
// case QUANT_SB_DIFF_LOSSY: 			if (num_csi >= 256) // FIX: this
// is the best case (we don't really know how many csi will be dropped)
// return true; 			break; 		case
// QUANT_SB_EXCURSION_LOSSY: 			if (num_csi >= 512) // FIX:
// scale on 'm' 				return true;
// break; 		case QUANT_MB_EXCURSION_LOSSY: 			if
// (num_csi >= 512) // FIX: scale on 'm' and 'num_quant_level'
// return true; 		case QUANT_MBR_LOSSLESS: if (num_csi
//>= 128)	// minimal limit for worst case
//return true; 		case QUANT_MBE_LOSSY: 			if (num_csi >=
// 192)	// minimal limit for worst case 				return
// true; 		default: 			break;
//	}
//	return false;
//}

/*!
 * \brief Remove dropped CSIs from the key, corresponding to the lossy indexes
 *
 * \param[in]		qparams				quantization parameters
 * \param[in, out]	key					key to remove
 * bits from
 * \param[in]		key_size_in_bits	size of the key in bits
 * \param[in]		indexes				array of indexes to
 * remove from the key
 * \param[in]		num_indexes			number of indexes in the
 * array
 *
 * \returns the new size of the key in bits
 */
uint32_t quant_dropp_csis(quant_lossy_params_t *qparams, uint8_t *key,
                          size_t key_size_in_bits, quant_index_t *indexes,
                          size_t num_indexes) {
  if (qparams->dynamic)
    return -1;

  uint32_t new_size = key_size_in_bits;
  for (size_t i = num_indexes - 1; i > 0; i--) {
    if (indexes[i] >= key_size_in_bits)
      break;
    for (int j = 0; j < qparams->nbits_per_sample; j++) {
      key_size_in_bits = shift_key_bits_left(key, key_size_in_bits, indexes[i]);
      add_bit_to_key(key, key_size_in_bits + 1, key_size_in_bits,
                     0); // fill with zero on right
    }

    new_size -= qparams->nbits_per_sample;
  }
  // last round
  if (indexes[0] < key_size_in_bits) {
    for (int j = 0; j < qparams->nbits_per_sample; j++) {
      key_size_in_bits = shift_key_bits_left(key, key_size_in_bits, indexes[0]);
      add_bit_to_key(key, key_size_in_bits + 1, key_size_in_bits, 0);
    }
    new_size -= qparams->nbits_per_sample;
  }

  return new_size;
}

uint32_t quant_retain_csis(quant_lossy_params_t *qparams, uint8_t *key,
                           size_t key_size_in_bits, quant_index_t *indexes,
                           size_t num_indexes) {
  size_t tmp_cap_in_bits = 64 * 8;
  uint8_t tmp_key[64] = {0};

  if (!quant_index_t_is_sorted(indexes, num_indexes)) {
    quant_index_t_sort(indexes, num_indexes);
  }

  size_t num_retain_bits = 0;
  // for (size_t i=0; i<key_size_in_bits; i+=qparams->nbits_per_sample) {
  //   bool retain_cur = false;
  //   quant_index_t cur_bit_idx = i / qparams->nbits_per_sample;
  //   for (size_t j=0; j<num_indexes; j++) {
  //     if (indexes[j] == cur_bit_idx) {
  //       retain_cur = true;
  //       break;
  //     }
  //   }

  //  if (retain_cur) {
  //    for (size_t j=0; j<qparams->nbits_per_sample; j++) {
  //      uint8_t cur = get_bit_at(key, key_size_in_bits, i+j);
  //      add_bit_to_key(tmp_key, tmp_cap_in_bits, num_retain_bits, cur);
  //      num_retain_bits++;
  //    }
  //  }
  //}

  for (size_t i = 0; i < num_indexes; i++) {
    if (i >= key_size_in_bits)
      break;

    size_t cur_bit_idx = indexes[i] * qparams->nbits_per_sample;
    for (size_t j = 0; j < qparams->nbits_per_sample; j++) {
      uint8_t cur = get_bit_at(key, key_size_in_bits, cur_bit_idx + j);
      add_bit_to_key(tmp_key, tmp_cap_in_bits, num_retain_bits, cur);
      num_retain_bits++;
    }
  }

  memcpy(key, tmp_key, (num_retain_bits + 8 - 1) / 8);

  return num_retain_bits;
}

/*!
 *  \brief simple insertion sort algorithm
 */
void csi_indexes_sort(quant_index_t *indexes, size_t num_indexes) {
  size_t i, j;
  quant_index_t key;
  for (i = 1; i < num_indexes; i++) {
    key = indexes[i];
    j = i - 1;

    // Move elements of arr[0..i-1],
    // that are greater than key,
    // to one position ahead of their
    // current position
    while (j >= 0 && indexes[j] > key) {
      indexes[j + 1] = indexes[j];
      j = j - 1;
    }
    indexes[j + 1] = key;
  }
}

/*!
 * \brief Merge CSI indexes array without duplicating
 *
 * \param[in, out]	indexes				array of indexes to
 * merge (not sorted)
 * \param[in, out]	actual_num_indexes	number of indexes in the array
 * \param[in]		indexes_capacity	allocated capacity of the
 * `indexes` array
 * \param[in]		new_indexes			array of new indexes to
 * merge (not sorted)
 * \param[in]		num_new_indexes		number of new indexes to merge
 */
uint32_t quant_merge_csi_indexes(quant_index_t *indexes,
                                 size_t *actual_num_indexes,
                                 size_t indexes_capacity,
                                 quant_index_t *new_indexes,
                                 size_t num_new_indexes) {
  if (*actual_num_indexes + num_new_indexes > indexes_capacity)
    return 0;

  size_t original_num_indexes = *actual_num_indexes;
  size_t cur_num_indexes = original_num_indexes;
  for (size_t i = 0; i < num_new_indexes; i++) {
    bool exists = false;
    for (size_t j = 0; j < original_num_indexes; j++) {
      if (indexes[j] == new_indexes[i]) {
        exists = true;
        break;
      }
    }

    if (!exists) {
      indexes[cur_num_indexes++] = new_indexes[i];
    }
  }

  quant_index_t_sort(indexes, cur_num_indexes);

  *actual_num_indexes = cur_num_indexes;

  return cur_num_indexes;
}

uint32_t quant_inter_csi_indexes(quant_index_t *indexes,
                                 size_t *actual_num_indexes,
                                 size_t indexes_capacity,
                                 quant_index_t *new_indexes,
                                 size_t num_new_indexes) {
  if (*actual_num_indexes + num_new_indexes > indexes_capacity)
    return 0;

  size_t original_num_indexes = *actual_num_indexes;
  size_t cur_num_indexes = original_num_indexes;
  for (size_t i = 0; i < num_new_indexes; i++) {
    bool exists = false;
    size_t j;
    for (j = 0; j < original_num_indexes; j++) {
      if (indexes[j] == new_indexes[i]) {
        exists = true;
        break;
      }
    }

    if (!exists) {
      // this function decrements cur_num_indexes
      quant_index_t_remove_at(indexes, &cur_num_indexes, j);
    }
  }

  quant_index_t_sort(indexes, cur_num_indexes);

  *actual_num_indexes = cur_num_indexes;

  return cur_num_indexes;
}

// int quant_complement_csi_indexes(quant_index_t *indexes, size_t
// *actual_num_indexes, size_t indexes_capacity, size_t num_csi)
//{
//   if (num_csi - *actual_num_indexes > indexes_capacity)
//     return -1;
//
//   quant_index_t complement[800];  // FIX: fix with define MAX_QUANT_INDEXES
//   (1600 / 2)
//
//   size_t cur_num_indexes = 0;
//   for (size_t i=0; i<num_csi; i++) {
//     bool exists = false;
//     for (size_t j=0; j<*actual_num_indexes; j++) {
//       if (indexes[j] == i) {
//         exists = true;
//         break;
//       }
//     }
//     if (!exists) {
//       complement[cur_num_indexes++] = i;
//     }
//   }
//
//   memcpy(indexes, complement, cur_num_indexes * sizeof(quant_index_t));
//   if (cur_num_indexes < *actual_num_indexes) {
//     // zeroize extra memory which is not used anymore
//     memset(&indexes[cur_num_indexes], 0, (*actual_num_indexes -
//     cur_num_indexes) * sizeof(quant_index_t));
//   }
//   *actual_num_indexes = cur_num_indexes;
//   return cur_num_indexes;
// }

void quant_excursion_select_random_subset(quant_index_t *excursions,
                                          size_t *num_excursions) {
  // TODO: Select a random subset of excursions to keep (need to define a limit
  // on the number of excursion to keep)
}

// bool quant_sb_excursion_get_indexes(quant_excursion_index_t *excursions,
// size_t num_excursions, quant_index_t *indexes, size_t *num_indexes)
//{
//	if (*num_indexes < num_excursions)
//		return false;
//
//	for (size_t i = 0; i < num_excursions; i++) {
//		indexes[i] = excursions[i].index;
//	}
//
//	return true;
// }
//
// bool quant_sb_excursion_reconstruct_excursions(quant_index_t *indexes, size_t
// num_indexes, quant_excursion_index_t *excursions, size_t *num_excursions)
//{
//	// TODO: Modify the `excursions` array to select the excursions matching
// the valid `indexes` 	return false;
// }

int quant_sb_lossless(csi_t *measures, size_t n_measures, uint8_t *key,
                      size_t key_size) {
  size_t n_quantized = (key_size * 8 < n_measures) ? key_size * 8 : n_measures;
  float mean = rssi_mean(measures, n_measures);

  for (int i = 0; i < n_quantized; i++) {
    if ((float)measures[i] >= mean)
      add_bit_to_key(key, n_quantized, i, 1);
    else
      add_bit_to_key(key, n_quantized, i, 0);
  }
  return n_quantized;
}

int quant_sb_lossy(csi_t *measures, size_t n_measures, uint8_t *outkey,
                   size_t key_size, quant_index_t *indexes,
                   size_t *num_indexes) {
  int num_quantized = 0;
  float mean = rssi_mean(measures, n_measures);

  uint32_t n_dropped = 0;
  uint32_t max_to_drop = *num_indexes;
  for (size_t i = 0; i < n_measures - 1; i++) {
    uint8_t bitval = 0;
    if ((float)measures[i] > mean + QP_SB_RESOLUTION)
      bitval = 1;
    else if ((float)measures[i] < mean - QP_SB_RESOLUTION)
      bitval = 0;
    else if (n_dropped >= max_to_drop)
      // detect too small `lossy_points` array
      return -1;
    else {
      indexes[n_dropped++] = i;
    }
    if (num_quantized > key_size * 8)
      break;
    add_bit_to_key(outkey, key_size * 8, i, bitval);
    num_quantized++;
  }
  *num_indexes = n_dropped;

  return num_quantized;
}

#define QUANT_MBE_NUM_MAX_LEVEL 16
// the pycom implementation here has a problem where it can has
// an odd number of quantization levels, resulting in a non
// equiprobalistic bit distribution for the current window.
// For example if num_level = 3, we only assign 00, 01, 10 and never 11
int quant_mbe_lossy(csi_t *measures, size_t n_measures, float alpha,
                    uint8_t *outkey, size_t key_size,
                    quant_index_t *lossy_points, size_t *num_lossy) {
  // function have been coded to only handle blockwise quant,
  // so we need some checks to ensure it works only for this case
  if (n_measures > QUANT_MBE_WINDOW_SIZE)
    return 0;

  // float entropy = csis_entropy(measures, n_measures);
  // uint8_t nbits_per_level = (uint8_t) (-entropy);  // truncate to decimal
  // size_t n_levels = (1 << nbits_per_level); // pow(2, x) <=> (1 << x)
  unsigned char level;
  uint8_t gen_bits;
  // uint8_t nbr_of_generated_bits_by_char = 0, key_char_index = 0;
  uint8_t key_capacity_in_bits = key_size * 8;
  int num_quantized = 0;
  // int8_t rest_bits;

  // computing density
  struct density density = PHYSEC_quntification_get_density(measures);

  // computing level number
  int8_t qunatification_level_nbr =
      PHYSEC_quntification_compute_level_nbr(&density);
  if (qunatification_level_nbr < 2)
    return 0; // don't quantize window if only one level
  if (qunatification_level_nbr % 2 != 0)
    return 0; // abort on non equiprobalistic bit distribution
  if (qunatification_level_nbr > 16)
    qunatification_level_nbr = 16;
  // else if (qunatification_level_nbr < 2) qunatification_level_nbr = 2;

  // computing thresholds
  csi_t threshold_starts[QUANT_MBE_NUM_MAX_LEVEL];
  csi_t threshold_ends[QUANT_MBE_NUM_MAX_LEVEL];
  double cdf = 0;
  for (int i = 0; i < qunatification_level_nbr; i++) {
    threshold_starts[i] = PHYSEC_quntification_inverse_cdf(cdf, &density);
    cdf += (1 - alpha) / qunatification_level_nbr;
    threshold_ends[i] = PHYSEC_quntification_inverse_cdf(cdf, &density);
    cdf += alpha / (qunatification_level_nbr - 1);
  }

  // quantification
  gen_bits = (uint8_t)log2(qunatification_level_nbr);
  for (int i = 0; i < n_measures; i++) {
    level = PHYSEC_quntification_get_level(measures[i], threshold_starts,
                                           threshold_ends,
                                           qunatification_level_nbr);
    if (level > 0) {
      level--;
      add_bits_to_key(outkey, key_capacity_in_bits, num_quantized, &level,
                      gen_bits);
      num_quantized += gen_bits;

      // rest_bits = gen_bits - (8-nbr_of_generated_bits_by_char);
      // if(rest_bits>0){
      //     outkey[key_char_index] += level>>rest_bits;
      //     key_char_index++;
      //     if(key_char_index==16){
      //         return 128;
      //     }
      //     outkey[key_char_index] += level<<(8+gen_bits-rest_bits);
      //     nbr_of_generated_bits_by_char = rest_bits;
      // }else{
      //     outkey[key_char_index] +=
      //     level<<(8-nbr_of_generated_bits_by_char-gen_bits);
      //     nbr_of_generated_bits_by_char += gen_bits;
      // }

    } else if (num_lossy) {
      (*num_lossy)++;
    }
  }

  PHYSEC_quntification_free_density(&density);

  return num_quantized; // 8*key_char_index+nbr_of_generated_bits_by_char;
                        // //num_quantized;
}

// This function is used by Alice before quantization to check whether the
// number of measurements she has will allow her to generate 128 bits, based on
// the number of bits per level.
bool measurements_sufficiency_check_mbr_lossless(int16_t *measures,
                                                 size_t nb_measures) {
  if (measures == NULL || nb_measures == 0)
    return false;

  int16_t max = i16_max(measures, nb_measures);
  int16_t min = i16_min(measures, nb_measures);

  int range = max - min;
  int nbits_per_level;

  if (range >= 0 && range < 16) {
    double s = sqrt((double)range);
    nbits_per_level = (int)s;
    nbits_per_level = (nbits_per_level > 0) ? nbits_per_level : 1;
  } else {
    nbits_per_level = 4;
  }

  return (nbits_per_level * nb_measures >= 128);
}

int quant_mbr_lossless(int16_t *measures, size_t n_measures, uint8_t *outkey,
                       size_t key_size) {
  size_t nbits_per_level, range, num_quantized;
  int16_t max, min;
  float interval;

  max = i16_max(measures, n_measures);
  min = i16_min(measures, n_measures);
  range = max - min;
  if (range >= 0 && range < 16) {
    nbits_per_level = sqrt(range);
    nbits_per_level = (nbits_per_level) ? nbits_per_level : 1;
  } else {
    nbits_per_level = 4;
  }
  tm_plog(TS_ON, VLEVEL_L, "> The number of bits per level = %d\n\r",
          nbits_per_level);

  size_t n_levels = (1 << nbits_per_level);
  interval = (float)range / (float)n_levels;
  tm_plog(TS_ON, VLEVEL_L, "> Interval = %f\n\r", interval);
  num_quantized = 0;
  for (size_t i = 0; i < n_measures; i++) {
    for (size_t j = 0; j < n_levels; j++) {
      float lbound = min + interval * j; // lower bound
      float hbound;                      // higher bound
      if (j == n_levels - 1)
        hbound = max + 1;
      else
        hbound = min + interval * (j + 1);
      if (measures[i] >= lbound && measures[i] < hbound) {
        uint8_t tmp = graycode_u8(j);
        add_bits_to_key(outkey, key_size * 8, num_quantized, &tmp,
                        nbits_per_level);
        num_quantized += nbits_per_level;
        break;
      }
    }
  }

  return num_quantized;
}

int quant_sb_diff_lossy(int16_t *measures, size_t n_measures, uint8_t *outkey,
                        size_t key_size, quant_index_t *lossy_points,
                        size_t *num_lossy) {
  size_t num_quantized = 0;

  uint32_t n_dropped = 0;
  uint32_t max_to_drop = *num_lossy;
  for (size_t i = 0; i < n_measures - 1; i++) {
    uint8_t bitval = 0;
    if (measures[i + 1] > measures[i] + QP_DIFFERENTIAL_RESOLUTION)
      bitval = 1;
    else if (measures[i + 1] < measures[i] - QP_DIFFERENTIAL_RESOLUTION)
      bitval = 0;
    else if (n_dropped >= max_to_drop)
      // detect too small `lossy_points` array
      return -1;
    else {
      lossy_points[n_dropped++] = i;
    }
    if (num_quantized > key_size * 8)
      break;
    add_bit_to_key(outkey, key_size * 8, i, bitval);
    num_quantized++;
  }
  *num_lossy = n_dropped;

  return (int)num_quantized;
}

int quant_adaptive(int16_t *measures, size_t n_measures, uint8_t *outkey,
                   size_t key_size, quant_index_t *lossy_points,
                   size_t *num_lossy) {
  return -1;
}

// int quant_sb_excursion_get_excursions(int16_t *measures, size_t n_measures,
// quant_excursion_index_t *excursions, size_t *num_excursions, uint8_t m, float
// alpha, size_t key_size)
//{
//	uint8_t variance = rssi_variance(measures, n_measures);
//	int16_t mean = rssi_mean(measures, n_measures);
//	int16_t q_plus = mean + (int16_t) (alpha * variance);
//	int16_t q_minus = mean - (int16_t) (alpha * variance);
//	size_t num_actual_excursions = 0;
//	for (int i=0; i<n_measures-m; i++) {
//		if (measures[i] <= q_plus && measures[i] >= q_minus) {
//			// the value belongs to the guard band
//			continue;
//		}
//		int breaked_at = -1;
//		bool positive = (measures[i] > q_plus) ? true : false;
//		for (int j=1; j<m; j++) {
//			if (positive && measures[i+j] <= q_plus
//			|| !positive && measures[i+j] >= q_minus) {
//				breaked_at = i+j;
//				break;
//			}
//		}
//		if (breaked_at != -1) {
//			// we skip values that are not in the same
//			// section (positive, guard band or negative)
//			// as the breaking value
//			// i = breaked_at-1;  // this is an optimization which
// is
//								  // not present
// in the paper's algorithm
//								  // so its
// commented 			continue;
//		}
//
//		//excursions[num_excursions].value = (uint8_t) positive;
//		excursions[num_actual_excursions].value = (positive) ? 1 : 0;
//		excursions[num_actual_excursions].index = (i*2+m-1) / 2;
//// (i + i_end) / 2 		num_actual_excursions++; 		i +=
///m-1;
//	}
//	if (num_actual_excursions > *num_excursions) {
//		// TODO: truncate excursions instead of returning an error (If
// there is still enough excursion to generate a key) 		return -1;
//	}
//
//	if (num_actual_excursions < key_size*8)
//		return -1;
//
//	quant_excursion_select_random_subset(excursions,
//&num_actual_excursions);
//
//	*num_excursions = num_actual_excursions;
//
//	return num_actual_excursions;
// }

// int quant_sb_excursion_get_valid_excursions_from_indexes(int16_t *measures,
// size_t n_measures, quant_index_t *excursions_indexes, size_t num_indexes,
// quant_excursion_index_t *excursions, size_t *num_excursions, uint8_t m, float
// alpha, size_t key_size)
//{
//	// TODO: Quantize only the measures corresponding excursion indexes,
// modify `excursion_indexes`, fill the `excursion` array
//
//	int16_t variance = rssi_variance(measures, n_measures);
//	int16_t mean = rssi_mean(measures, n_measures);
//	int16_t q_plus = mean + (int16_t) (alpha * variance);
//	int16_t q_minus = mean - (int16_t) (alpha * variance);
//	bool m_even = (m % 2) ? false : true;
//
//	if (num_indexes < key_size*8 || *num_excursions < key_size*8 ||
//*num_excursions < num_indexes) 		return -1;
//
//	size_t num_actual_excursions = 0;
//	for (int i=0; i < num_indexes; i++) {
//		quant_index_t cur_idx = excursions_indexes[i];
//		int left_part_size = (int)roundf((float) (m-1) / 2);
//		int right_part_size = (int)roundf((float)m/2);
//		if ((measures[cur_idx] <= q_plus && measures[cur_idx] >=
// q_minus)
//			|| cur_idx < left_part_size
//			|| cur_idx > right_part_size) {
//			continue;
//		}
//		int breaked_at = -1;
//		int start = (int)(cur_idx - left_part_size);
//		int end = (int)(cur_idx + right_part_size);
//		bool positive = (measures[start] > q_plus) ? true : false;
//		for (int j=start+1; j<end; j++) {
//			if (positive && measures[i+j] <= q_plus
//			|| !positive && measures[i+j] >= q_minus) {
//				breaked_at = i+j;
//				break;
//			}
//		}
//		if (breaked_at != -1)
//			continue;
//
//		excursions[num_actual_excursions].index = cur_idx;
//		excursions[num_actual_excursions].value = (positive) ? 1 : 0;
//		num_excursions++;
//		if (num_actual_excursions >= key_size*8)
//			break;
//	}
//	if (num_actual_excursions < key_size*8)
//		return -1;
//	return num_actual_excursions;
// }

// int quant_sb_excursion_quantize(quant_excursion_index_t *excursions, size_t
// num_excursions, uint8_t *outkey, size_t key_size)
//{
//	if (num_excursions <= key_size * 8)
//		return -1;
//
//	for (int i=0; i<key_size*8; i++) {
//		add_bit_to_key(outkey, key_size*8, i, excursions[i].value);
//	}
//
//	return -1;
// }

int quant_sb_excursion_quantize2(csi_t *measures, size_t n_measures,
                                 quant_index_t *indexes, size_t *num_indexes,
                                 uint8_t m, float alpha, uint8_t *outkey,
                                 size_t key_size) {
  if (n_measures < m)
    return -1;

  float variance = rssi_variance(measures, n_measures);
  float mean = rssi_mean(measures, n_measures);
  float q_plus = mean + (float)(alpha * variance);
  float q_minus = mean - (float)(alpha * variance);
  size_t num_actual_excursions = 0;
  size_t num_bits_added = 0;
  size_t key_capacity_in_bits = key_size * 8;
  for (size_t i = 0; i < n_measures - m; i++) {
    if (i >= key_capacity_in_bits) {
      break;
    }

    if ((float)measures[i] <= q_plus && (float)measures[i] >= q_minus) {
      // the value belongs to the guard band
      // we put 0 in the key (will be remove later)
      add_bit_to_key(outkey, key_capacity_in_bits, i, 0);
      num_bits_added++;
      continue;
    }
    int breaked_at = -1;
    bool positive = ((float)measures[i] > q_plus) ? true : false;
    for (int j = 1; j < m; j++) {
      if ((positive && (float)measures[i + j] <= q_plus) ||
          (!positive && (float)measures[i + j] >= q_minus)) {
        breaked_at = i + j;
        break;
      }
    }
    if (breaked_at == -1) {
      if (num_actual_excursions >= *num_indexes) {
        return -1;
      }
      // if valid excursion, we had the center
      // index to excursion indexes array
      int mid_index = (i + i + m - 1) / 2;
      indexes[num_actual_excursions++] = mid_index;
      i += m - 1;
      // add excursion bits
      for (int j = 0; j < m; j++) {
        add_bit_to_key(outkey, key_capacity_in_bits, i + j, (uint8_t)positive);
      }
      num_bits_added += m;
      continue;
    }
    // add current bit (not part of an
    // excursion)
    add_bit_to_key(outkey, key_capacity_in_bits, i, (uint8_t)positive);
    num_bits_added++;
  }

  *num_indexes = num_actual_excursions;

  return n_measures;
}

int quant_mb_excursion_quantize2(quant_lossy_params_t *qparams, csi_t *measures,
                                 size_t n_measures, quant_index_t *indexes,
                                 size_t *num_indexes, uint8_t m,
                                 uint8_t *outkey, size_t key_size) {
  // PAPER ASSUMPTION: range between max(measures) and min(measures) is over 16
  if (n_measures < m)
    return -1;
  int16_t max = i16_max(measures, n_measures);
  int16_t min = i16_min(measures, n_measures);
  int16_t range = max - min;

  if (range < 16) {
    return -1;
  }

  uint8_t nbits_per_level = 4;
  int16_t interval = range / 16;

  size_t num_actual_excursions = 0;
  size_t num_bits_added = 0;
  size_t key_capacity_in_bits = key_size * 8;

  for (size_t i = 0; i < n_measures - m; i++) {
    if (i * nbits_per_level >= key_capacity_in_bits) {
      // key full
      break;
    }
    int16_t interval_min = min;
    int16_t interval_max = max;
    size_t j;
    // retrieve current intervals for measures[i]
    for (j = 0; j < 16; j++) {
      int16_t cur_interval_min = min + j * interval;
      int16_t cur_interval_max = min + (j + 1) * interval;
      if (j == 15) {
        cur_interval_max = max + 1;
      }
      if (measures[i] >= cur_interval_min && measures[i] < cur_interval_max) {
        interval_min = cur_interval_min;
        interval_max = cur_interval_max;
        break;
      }
    }
    int breaked_at = -1;
    // check consecutive quant values
    for (size_t k = i; k < i + m; k++) {
      if (measures[k] < interval_min || measures[k] >= interval_max) {
        breaked_at = k;
        break;
      }
    }

    uint8_t tmp = graycode_u8(j);
    if (breaked_at == -1) {
      int mid_index = (i + i + m - 1) / 2;
      indexes[num_actual_excursions++] = mid_index;
      for (size_t k = 0; k < m; k++) {
        add_bits_to_key(outkey, key_capacity_in_bits, i * nbits_per_level, &tmp,
                        nbits_per_level);
        num_bits_added += nbits_per_level;
      }
      continue;
    }
    add_bits_to_key(outkey, key_capacity_in_bits, i * nbits_per_level, &tmp,
                    nbits_per_level);
    num_bits_added += nbits_per_level;
  }

  qparams->dynamic = false;
  qparams->nbits_per_sample = nbits_per_level;

  *num_indexes = num_actual_excursions;

  return num_bits_added;
}
