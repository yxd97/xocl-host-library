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
    #pragma HLS INTERFACE m_axi port=values         offset=slave bundle=gmem_mat1
    #pragma HLS INTERFACE m_axi port=col_idx        offset=slave bundle=gmem_mat2
    #pragma HLS INTERFACE m_axi port=row_ptr        offset=slave bundle=gmem_mat3
    #pragma HLS INTERFACE m_axi port=vector_in      offset=slave bundle=gmem_vec1
    #pragma HLS INTERFACE m_axi port=vector_out     offset=slave bundle=gmem_vec2

    // initialize vector_out
    for (unsigned i = 0; i < num_rows; i++) {
        vector_out[i] = 0;
    }

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
