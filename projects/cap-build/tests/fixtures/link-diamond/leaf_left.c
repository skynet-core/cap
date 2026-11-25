#include <zlib.h>
int leaf_left(void) { return zlibVersion()[0] != 0 ? 19 : -999; }
