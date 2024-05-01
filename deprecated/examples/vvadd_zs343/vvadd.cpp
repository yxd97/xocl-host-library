#include <hls_stream.h>
#include "vvadd.h"

static void load_input(int* in, hls::stream<int>& inStream, int size) {
    mem_rd: for (int i = 0; i < size; i++) {
        inStream.write(in[i]);
    }
}

static void compute_add(hls::stream<int>& in1_stream,
                        hls::stream<int>& in2_stream,
                        hls::stream<int>& out_stream,
                        int size) {
    execute: for (int i = 0; i < size; i++) {
        out_stream.write((in1_stream.read() + in2_stream.read()));
    }
}

static void store_result(int* out, hls::stream<int>& out_stream, int size) {
    mem_wr: for (int i = 0; i < size; i++) {
        out[i] = out_stream.read();
    }
}

extern "C" {
void vvadd(int* in1, int* in2, int* out, int size) {
    #pragma HLS INTERFACE m_axi port = in1 bundle = gmem0
    #pragma HLS INTERFACE m_axi port = in2 bundle = gmem1
    #pragma HLS INTERFACE m_axi port = out bundle = gmem0

    static hls::stream<int> in1_stream("input_stream_1");
    static hls::stream<int> in2_stream("input_stream_2");
    static hls::stream<int> out_stream("output_stream");

    #pragma HLS dataflow
    load_input(in1, in1_stream, size);
    load_input(in2, in2_stream, size);
    compute_add(in1_stream, in2_stream, out_stream, size);
    store_result(out, out_stream, size);
}
}
