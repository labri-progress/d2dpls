#include "acquisition.h"
#include <stdlib.h>

inline int16_t normalize_csi(int16_t csi) { return (int16_t)abs((int)csi); }
