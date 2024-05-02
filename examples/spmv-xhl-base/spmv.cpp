const unsigned FP_ADD_LATENCY = 10;

extern "C"  void spmv (
    const float* values,
    const unsigned* col_idx,
    const unsigned* row_ptr,
    const float* vector_in,
    float* vector_out,

    const unsigned num_rows,
    const unsigned num_cols
) {
    #pragma HLS interface m_axi port=values         offset=slave bundle=gmem_mat1
    #pragma HLS interface m_axi port=col_idx        offset=slave bundle=gmem_mat2
    #pragma HLS interface m_axi port=row_ptr        offset=slave bundle=gmem_mat3
    #pragma HLS interface m_axi port=vector_in      offset=slave bundle=gmem_vec1
    #pragma HLS interface m_axi port=vector_out     offset=slave bundle=gmem_vec2

    // initialize vector_out
    for (unsigned i = 0; i < num_rows; i++) {
        #pragma HLS pipeline II=1
        vector_out[i] = 0.0;
    }

    for (unsigned row_idx = 0; row_idx < num_rows; row_idx++) {
        #pragma HLS pipeline off
        unsigned start = row_ptr[row_idx];
        unsigned end = row_ptr[row_idx + 1];

        // accumulation interleaving for II=1
        float acc[FP_ADD_LATENCY] = {0.0};
        #pragma HLS array_partition variable=acc complete
        #pragma HLS bind_op variable=acc op=fadd latency=FP_ADD_LATENCY

        for (unsigned i = start; i < end; i++) {
            #pragma HLS pipeline II=1 style=flp
            unsigned idx = col_idx[i];
            acc[(i - start) % FP_ADD_LATENCY] += values[i] * vector_in[idx];
        }

        float res = 0.0;
        for (unsigned i = 0; i < FP_ADD_LATENCY; i++) {
            #pragma HLS pipeline II=1
            res += acc[i];
        }
        vector_out[row_idx] = res;
    }
}
