#include "ap_int.h"
#include "hls_stream.h"

const unsigned pack_size = 8; // must be 8
const unsigned vb_bank_size = 1024;
const unsigned ob_bank_size = 1024;

typedef ap_uint<pack_size*32*2> hbm_word_t;
typedef ap_ufixed<32, 8, AP_RND, AP_SAT> value_t;
typedef ap_uint<32> index_t;

/* HBM word format, K == pack_size:
* partition metadata:
         MSB
word n    |  undef  |  undef  |  undef  |  undef  | ... |  undef  |  start  |
word n+1  |  undef  | len K-1 |  undef  | len K-2 | ... |  undef  |  len 0  |

* matrix:
         MSB
word n    | val K-1 | idx K-1 | val K-2 | idx K-2 | ... |  val 0  |  idx 0  |

* vector:
word n    | val 2K-1 | val 2K-2 |  ... |  val 1  |  val 0  |
*/

namespace intepret_hbm_word {

index_t as_metadata_start (const hbm_word_t word) {
    #pragma HLS inline
    return word(31, 0);
}

index_t as_metadata_length (const hbm_word_t word, const unsigned k) {
    #pragma HLS inline
    return word(64 * k + 31, 64 * k);
}

index_t as_data_index (const hbm_word_t word, const unsigned k) {
    #pragma HLS inline
    return word(64 * k + 31, 64 * k);
}

value_t as_data_value (const hbm_word_t word, const unsigned k) {
    #pragma HLS inline
    return word(64 * k + 63, 64 * k + 32);
}

value_t as_vector (const hbm_word_t word, const unsigned k) {
    #pragma HLS inline
    return word(32 * k + 31, 32 * k);
}

void set_vector (hbm_word_t &word, const unsigned k, const value_t value) {
    #pragma HLS inline
    word(32 * k + 31, 32 * k) = value;
}

} // namespace intepret_hbm_word

const index_t next_row_marker = 0xffffffff;

typedef ap_uint<2> token_t;
#define token_none  0
#define token_start 1
#define token_next  2
#define token_exit  3

struct NonZero {
    value_t data;
    index_t row_index;
    index_t col_index;
    token_t token;
};

struct Update {
    value_t data;
    index_t row_index;
    token_t token;
};


//==============================================================================
// Matrix loader
//==============================================================================

template<unsigned pack_size>
struct PartitionInfo {
    index_t start;
    index_t length[pack_size];
    index_t max_length;
};

// read metadata
template<unsigned pack_size>
PartitionInfo<pack_size> read_partition_table (
    const hbm_word_t *matrix,
    const index_t partition_id
) {
    #pragma HLS inline
    hbm_word_t pp_word[2];
    #pragma HLS array_partition variable=pp_word complete
    for (int i = 0; i < 2; i++) {
        #pragma HLS pipeline II=1
        pp_word[i] = matrix[2 * partition_id + i];
    }
    PartitionInfo<pack_size> ret;
    ret.start = intepret_hbm_word::as_metadata_start(pp_word[0]);
    for (int k = 0; k < pack_size; k++) {
        #pragma HLS unroll
        ret.length[k] = intepret_hbm_word::as_metadata_length(pp_word[1], k);
    }
    ret.max_length = 0;
    for (int k = 0; k < pack_size; k++) {
        #pragma HLS unroll
        if (ret.length[k] > ret.max_length) {
            ret.max_length = ret.length[k];
        }
    }
    return ret;
}

template<unsigned pack_size>
void matrix_loader (
    const hbm_word_t *matrix,
    const index_t row_partition_id,
    const index_t num_col_partitions,
    const index_t total_num_partitions,
    hls::stream<NonZero> nz_streams[pack_size]
) {
    index_t matrix_data_offset = total_num_partitions * 2;
    for (int col_partition_id = 0; col_partition_id < num_col_partitions; col_partition_id++) {
        #pragma HLS pipeline off

        // read partition metadata
        PartitionInfo<pack_size> partition_info = read_partition_table<pack_size>(
            matrix, row_partition_id * num_col_partitions + col_partition_id
        );

        // initialize row index counter
        index_t row_idx_counter[pack_size];
        #pragma HLS array_partition variable=row_idx_counter complete
        for (int k = 0; k < pack_size; k++) {
            #pragma HLS unroll
            row_idx_counter[k] = k;
        }

        // attach start tokens
        for (int k = 0; k < pack_size; k++) {
            nz_streams[k].write(NonZero{0, 0, 0, token_start});
        }

        // read matrix data
        stream_read_pipeline: for (int i = 0; i < partition_info.max_length; i++) {
            #pragma HLS pipeline II=1
            hbm_word_t word = matrix[matrix_data_offset + partition_info.start + i];
            for (int k = 0; k < pack_size; k++) {
                #pragma HLS unroll
                if (i < partition_info.length[k]) {
                    if (intepret_hbm_word::as_data_index(word, k) == next_row_marker) {
                        value_t incr = pack_size * intepret_hbm_word::as_data_value(word, k);
                        row_idx_counter[k] += incr;
                    } else {
                        NonZero nz;
                        nz.data = intepret_hbm_word::as_data_value(word, k);
                        nz.row_index = row_idx_counter[k];
                        nz.col_index = intepret_hbm_word::as_data_index(word, k);
                        nz.token = token_none;
                        nz_streams[k].write(nz);
                    }
                }
            }
        }

        // attach next tokens
        for (int k = 0; k < pack_size; k++) {
            #pragma HLS unroll
            nz_streams[k].write(NonZero{0, 0, 0, token_next});
        }

    } // for col_partition_id

    // attach exit tokens
    for (int k = 0; k < pack_size; k++) {
        #pragma HLS unroll
        nz_streams[k].write(NonZero{0, 0, 0, token_exit});
    }
}

//==============================================================================
// Vector Buffer Access Unit
//==============================================================================

template<unsigned pack_size, unsigned vb_bank_size>
void read_unit (
    hls::stream<NonZero> &nz_stream,
    hls::stream<Update> &update_stream,
    value_t vector_buffer_bank[vb_bank_size]
) {
    bool exit = false;
    bool is_idle = true;
    while (!exit) {
        #pragma HLS pipeline II=1
        NonZero nz = nz_stream.read();
        if (is_idle) {
            if (nz.token == token_exit) {
                exit = true;
                update_stream.write(Update{0, 0, token_exit});
            } else if (nz.token == token_start) {
                is_idle = false;
                update_stream.write(Update{0, 0, token_start});
            }
        } else {
            if (nz.token == token_next) {
                exit = true;
                is_idle = true;
                update_stream.write(Update{0, 0, token_next});
            } else {
                value_t vector_value = vector_buffer_bank[
                    (nz.col_index / pack_size) % vb_bank_size
                ];
                Update ud;
                ud.data = nz.data * vector_value;
                ud.row_index = nz.row_index;
                ud.token = token_none;
                update_stream.write(ud);
            }
        }
    }
}

template<unsigned pack_size, unsigned vb_bank_size>
void wirte_unit (
    const hbm_word_t *vector_in,
    const index_t col_partition_id,
    value_t vector_buffer[pack_size][vb_bank_size]
) {
    const index_t num_words = vb_bank_size / pack_size / 2;
    const index_t read_offset = col_partition_id * num_words;
    for (int i = 0; i < num_words; i++) {
        #pragma HLS pipeline II=1
        hbm_word_t word = vector_in[col_partition_id * num_words + i];
        duplicate: for (int k = 0; k < pack_size; k++) {
            #pragma HLS unroll
            unpack: for (int kk = 0; kk < pack_size; kk++) {
                #pragma HLS unroll
                vector_buffer[k][i * pack_size + kk] = intepret_hbm_word::as_vector(word, kk);
            }
        }
    }
}

template<unsigned pack_size, unsigned vb_bank_size>
void vector_buffer_access_unit (
    hbm_word_t *vector_in,
    hls::stream<NonZero> nz_streams[pack_size],
    hls::stream<Update> update_streams[pack_size],
    const index_t num_col_partitions
) {
    value_t vector_buffer[pack_size][vb_bank_size];
    #pragma HLS ARRAY_PARTITION variable=vector_buffer complete dim=1
    #pragma HLS ARRAY_PARTITION variable=vector_buffer cyclic factor=pack_size dim=2

    for (int i = 0; i < num_col_partitions; i++) {
        #pragma HLS dataflow
        wirte_unit<pack_size, vb_bank_size>(vector_in, i, vector_buffer);
        for (int k = 0; k < pack_size; k++) {
            #pragma HLS unroll
            read_unit<pack_size, vb_bank_size>(
                nz_streams[k], update_streams[k], vector_buffer[k]
            );
        }
    }

    // consume the last exit token
    for (int k = 0; k < pack_size; k++) {
        #pragma HLS unroll
        nz_streams[k].read();
    }

    // attach exit tokens
    for (int k = 0; k < pack_size; k++) {
        #pragma HLS unroll
        update_streams[k].write(Update{0, 0, token_exit});
    }
}

//==============================================================================
// Process Engines
//==============================================================================

template<unsigned pack_size, unsigned ob_bank_size>
void pe (
    hls::stream<Update> &update_stream,
    value_t output_buffer_bank[ob_bank_size]
) {
    // reset output buffer
    for (int i = 0; i < ob_bank_size / 2; i++) {
        #pragma HLS pipeline II=1
        output_buffer_bank[2*i] = 0;
        output_buffer_bank[2*i + 1] = 0;
    }

    bool exit = false;
    while (!exit) {
        #pragma HLS pipeline II=1
        #pragma HLS dependence variable=output_buffer_bank inter false
        Update ud = update_stream.read();
        if (ud.token == token_exit) {
            exit = true;
        } else if (ud.token == token_start) {
            // do nothing
        } else if (ud.token == token_next) {
            // do nothing
        } else {
            index_t ob_bank_idx = ud.row_index % pack_size;
            output_buffer_bank[ob_bank_idx] += ud.data;
        }
    }
}

template<unsigned pack_size, unsigned ob_bank_size>
void result_drain_unit (
    value_t output_buffer[pack_size][ob_bank_size],
    const index_t num_elements,
    hbm_word_t *vector_out
) {
    const index_t num_words = num_elements / pack_size / 2;
    for (int i = 0; i < num_words; i++) {
        #pragma HLS pipeline II=1
        hbm_word_t word;
        for (int k = 0; k < pack_size; k++) {
            #pragma HLS unroll
            intepret_hbm_word::set_vector(word, k, output_buffer[k][2*i]);
            intepret_hbm_word::set_vector(word, k, output_buffer[k][2*i + 1]);
        }
        vector_out[i] = word;
    }

}

extern "C" {
void spmv (
    hbm_word_t *matrix,
    hbm_word_t *vector_in,
    hbm_word_t *vector_out,
    const index_t row_partition_id,
    const index_t partition_num_rows,
    const index_t num_col_partitions,
    const index_t total_num_partitions
) {
    #pragma HLS INTERFACE m_axi port=matrix offset=slave bundle=gmem_mat
    #pragma HLS INTERFACE m_axi port=vector_in offset=slave bundle=gmem_vec
    #pragma HLS INTERFACE m_axi port=vector_out offset=slave bundle=gmem_vec

    #pragma HLS dataflow

    value_t output_buffer[pack_size][ob_bank_size];
    #pragma HLS ARRAY_PARTITION variable=output_buffer complete dim=1

    hls::stream<NonZero> nz_streams[pack_size];
    #pragma HLS STREAM variable=nz_streams depth=2
    hls::stream<Update> update_streams[pack_size];
    #pragma HLS STREAM variable=update_streams depth=2

    matrix_loader<pack_size>(
        matrix,
        row_partition_id,
        num_col_partitions,
        total_num_partitions,
        nz_streams
    );

    vector_buffer_access_unit<pack_size, vb_bank_size>(
        vector_in,
        nz_streams,
        update_streams,
        num_col_partitions
    );

    for (int k = 0; k < pack_size; k++) {
        #pragma HLS unroll
        pe<pack_size, ob_bank_size>(update_streams[k], output_buffer[k]);
    }

    result_drain_unit<pack_size, ob_bank_size>(
        output_buffer,
        partition_num_rows,
        vector_out
    );
}
} // extern "C"