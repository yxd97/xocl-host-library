const unsigned FPADD_LATENCY = 8;

extern "C"  void spmv (
    const float* values,
    const unsigned* col_idx,
    const unsigned* row_ptr,
    float* vector_in,
    float* vector_out,

    const unsigned num_rows,
    const unsigned num_cols
) {
    #pragma HLS interface m_axi port=values         offset=slave bundle=gmem_mat1
    #pragma HLS interface m_axi port=col_idx        offset=slave bundle=gmem_mat2
    #pragma HLS interface m_axi port=row_ptr        offset=slave bundle=gmem_mat3
    #pragma HLS interface m_axi port=vector_in      offset=slave bundle=gmem_vec1
    #pragma HLS interface m_axi port=vector_out     offset=slave bundle=gmem_vec2

    for (unsigned row_idx = 0; row_idx < num_rows; row_idx++) {
        #pragma HLS pipeline off
        unsigned start = row_ptr[row_idx];
        unsigned end = row_ptr[row_idx + 1];

        float res = 0.0;
        for (unsigned i = start; i < end; i++) {
            #pragma HLS pipeline II=FPADD_LATENCY style=flp
            unsigned idx = col_idx[i];
            res += values[i] * vector_in[idx];
        }

        vector_out[row_idx] = res;
    }
}
