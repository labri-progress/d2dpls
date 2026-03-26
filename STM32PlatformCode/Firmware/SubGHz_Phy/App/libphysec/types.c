#include "types.h"

#include <stdlib.h>

struct density *alloc_density(uint16_t bin_nbr) {
  struct density *ptr = malloc(sizeof(struct density));
  if (!ptr)
    return NULL;
  ptr->bins = calloc(bin_nbr, sizeof(csi_t));
  if (!ptr->bins) {
    free(ptr);
    return NULL;
  }

  ptr->values = calloc(bin_nbr, sizeof(double));
  if (!ptr->values) {
    free(ptr->bins);
    free(ptr);
    return NULL;
  }

  ptr->bin_nbr = bin_nbr;
  ptr->q_0 = 0;

  return ptr;
}

void free_density(struct density *ptr) {
  if (ptr) {
    if (ptr->bins) {
      free(ptr->bins);
      ptr->bins = NULL;
    }
    if (ptr->values) {
      free(ptr->values);
      ptr->values = NULL;
    }
    free(ptr);
    ptr = NULL;
  }
}
