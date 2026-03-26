#pragma once

#include "types.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define QUANT_MBE_WINDOW_SIZE 10

#define gen_insert_sort(ty)                                                    \
  void ty##_sort(ty *arr, size_t num_elements) {                               \
    for (size_t i = 1; i < num_elements; i++) {                                \
      ty key = arr[i];                                                         \
      size_t j;                                                                \
      for (j = i; j > 0 && arr[j - 1] > key; j--) {                            \
        arr[j] = arr[j - 1];                                                   \
      }                                                                        \
      arr[j] = key;                                                            \
    }                                                                          \
  }

#define gen_quick_sort(ty)                                                     \
  void ty##_quick_sort(ty *arr, size_t num_elements) {                         \
    if (num_elements <= 1)                                                     \
      return;                                                                  \
    ty pivot = arr[num_elements / 2];                                          \
    ty *left = arr;                                                            \
    ty *right = arr + num_elements - 1;                                        \
    while (left <= right) {                                                    \
      if (*left < pivot) {                                                     \
        left++;                                                                \
        continue;                                                              \
      }                                                                        \
      if (*right > pivot) {                                                    \
        right--;                                                               \
        continue;                                                              \
      }                                                                        \
      ty temp = *left;                                                         \
      *left = *right;                                                          \
      *right = temp;                                                           \
      left++;                                                                  \
      right--;                                                                 \
    }                                                                          \
    ty##_quick_sort(arr, right - arr + 1);                                     \
    ty##_quick_sort(left, arr + num_elements - left);                          \
  }

#define gen_sort_check(ty)                                                     \
  bool ty##_is_sorted(ty *arr, size_t num_elements) {                          \
    for (size_t i = 1; i < num_elements; i++)                                  \
      if (arr[i - 1] > arr[i])                                                 \
        return false;                                                          \
    return true;                                                               \
  }

#define gen_array_remove_at(ty)                                                \
  void ty##_remove_at(ty *arr, size_t *num_elements, size_t index) {           \
    if ((*num_elements) - 1 != index)                                          \
      memcpy(&arr[index], &arr[index + 1],                                     \
             (*num_elements - index - 1) * sizeof(ty));                        \
    memset(&arr[*num_elements - 1], 0, sizeof(ty));                            \
    (*num_elements)--;                                                         \
  }

/** Array **/
extern void csi_t_remove_at(csi_t *arr, size_t *num_elements, size_t index);
extern void quant_index_t_remove_at(quant_index_t *arr, size_t *num_elements,
                                    size_t index);

/** Proba **/
extern struct density *compute_pdf(csi_t *csis, size_t num_csi);

extern struct density *compute_cdf(struct density *pdf);

/** sorting **/

extern void quant_index_t_sort(quant_index_t *arr, size_t num_elements);
extern void csi_t_sort(csi_t *arr, size_t num_elements);
extern void csi_t_quick_sort(csi_t *arr, size_t num_elements);
extern bool csi_t_is_sorted(csi_t *arr, size_t num_elements);
extern bool quant_index_t_is_sorted(quant_index_t *arr, size_t num_elements);

/** Pycom ported code **/
extern struct density PHYSEC_quntification_get_density(csi_t *rssi_window);

extern void PHYSEC_quntification_free_density(struct density *d);

extern int8_t PHYSEC_quntification_inverse_cdf(double cdf, struct density *d);

// <--- Density function estimation

extern int8_t PHYSEC_quntification_compute_level_nbr(struct density *d);

/*
    return value:
        level index strating from 1.
        0 : in case of error
*/
extern unsigned char
PHYSEC_quntification_get_level(csi_t rssi, csi_t *threshold_starts,
                               csi_t *threshold_ends,
                               csi_t qunatification_level_nbr);
