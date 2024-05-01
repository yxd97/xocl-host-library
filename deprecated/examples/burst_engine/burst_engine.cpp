#include <stdint.h>
#include "ap_int.h"

extern "C" {
void burst_engine(
    ap_int<512> *location,
    const uint64_t size_in_bytes,
    const int read_or_write // 1 for read, 0 for write
) {
    const uint64_t num_transfers = size_in_bytes / 64;
    if (read_or_write == 1) {
        ap_int<512> temp = 0;
        for (uint64_t i = 0; i < num_transfers; i++) {
            #pragma HLS PIPELINE II=1
            temp += location[i]; // to prevent optimization
        }
        location[0] = temp; // to prevent optimization
    } else {
        for (uint64_t i = 0; i < num_transfers; i++) {
            #pragma HLS PIPELINE II=1
            location[i] = i;
        }
    }
}
} // extern "C"