#include <hls_stream.h>

const unsigned P = 2;
const unsigned NUM_COLS = 107614;
extern "C" {
void spmv (
    const int* values,
    const int* col_idx,
    const int* row_ptr,
    const int* vector_in,
    int* vector_out,
    const unsigned num_rows,
    const unsigned num_cols
) {
    // #pragma HLS INTERFACE m_axi port=values         offset=slave bundle=gmem_mat1
    // #pragma HLS INTERFACE m_axi port=col_idx        offset=slave bundle=gmem_mat2
    // #pragma HLS INTERFACE m_axi port=row_ptr        offset=slave bundle=gmem_mat3
    // #pragma HLS INTERFACE m_axi port=vector_in      offset=slave bundle=gmem_vec1
    // #pragma HLS INTERFACE m_axi port=vector_out     offset=slave bundle=gmem_vec2
    // #pragma HLS INTERFACE s_axilite port=values     bundle=control
    // #pragma HLS INTERFACE s_axilite port=col_idx    bundle=control
    // #pragma HLS INTERFACE s_axilite port=row_ptr    bundle=control
    // #pragma HLS INTERFACE s_axilite port=vector_in  bundle=control
    // #pragma HLS INTERFACE s_axilite port=vector_out bundle=control
    // #pragma HLS INTERFACE s_axilite port=num_rows   bundle=control
    // #pragma HLS INTERFACE s_axilite port=num_cols   bundle=control
    // // #pragma HLS INTERFACE ap_ctrl_chain port=return
    // #pragma HLS INTERFACE s_axilite port=return     bundle=control

    // int vec_buf[P][NUM_COLS];
    // #pragma HLS array_partition variable=vec_buf dim=1 complete
    // for (unsigned i = 0; i < num_cols; i++) {
    //     #pragma HLS pipeline II=1
    //     for (unsigned k = 0; k < P; k++) {
    //         #pragma HLS unroll
    //         vec_buf[k][i] = vector_in[i];
    //     }
    // }


    // for (unsigned rid = 0; rid < num_rows; rid++) {
    //     unsigned start = row_ptr[rid];
    //     unsigned end = row_ptr[rid + 1];
    //     int acc = 0;
    //     // (end - start + 1) % P == 0
    //     for (unsigned i = start/P; i < end/P; i++) {
    //         #pragma HLS pipeline II=1
    //         for (unsigned k = 0; k < P; k++) {
    //             #pragma HLS unroll
    //             unsigned cid = col_idx[i * P + k];
    //             int mat_val = values[i * P + k];
    //             int vec_val = vec_buf[k][cid];
    //             acc += mat_val * vec_val;
    //         }
    //     }
    //     vector_out[rid] = acc;
    // }

    for (unsigned row_idx = 0; row_idx < num_rows; row_idx++) {
        unsigned start = row_ptr[row_idx];
        unsigned end = row_ptr[row_idx + 1];
        for (unsigned i = start; i < end; i++) {
            unsigned idx = col_idx[i];
            vector_out[row_idx] += values[i] * vector_in[idx];
        }
    }
}
}