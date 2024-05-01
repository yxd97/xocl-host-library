extern "C" void vvadd(
    const float* a,
    const float* b,
    float* c,
    const unsigned size
) {
    #pragma HLS INTERFACE m_axi port=a offset=slave bundle=gmem_a
    #pragma HLS INTERFACE m_axi port=b offset=slave bundle=gmem_b
    #pragma HLS INTERFACE m_axi port=c offset=slave bundle=gmem_c

    for (unsigned i = 0; i < size; ++i) {
        #pragma HLS PIPELINE II=1
        c[i] = a[i] + b[i];
    }
}
