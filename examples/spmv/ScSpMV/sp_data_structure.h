#ifndef LIBSW_SP_DATA_STRUCTURE_H
#define LIBSW_SP_DATA_STRUCTURE_H

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <typeinfo>
#include <unordered_map>

// #include "cnpy.h"

namespace spmv {
namespace io {

// helper function for generating randomly increasing sequence
std::vector<unsigned> rand_incr(unsigned min, unsigned max, unsigned len) {
    if (len > (max - min)) {
        printf("[ERROR]: cannot generate %d numbers from range [%d, %d)\n", len, min, max);
        fflush(stdout);
        throw std::invalid_argument("random range too small");
    }
    std::vector<unsigned> seq(len);
    std::unordered_map<unsigned, bool> used;
    for (size_t i = 0; i < len; i++) {
        unsigned x;
        do {
            x = rand() % (max - min) + min;
        } while (used[x]);
        used[x] = true;
        seq[i] = x;
    }
    std::sort(seq.begin(), seq.end());
    return seq;
}

//-------------------------------------------------------------------
// Compressed Sparse Row (CSR) format
//-------------------------------------------------------------------

// Data structure for csr matrix.
template<typename data_t>
class CSRMatrix {
public:
    /*! \brief The number of rows of the sparse matrix */
    uint32_t num_rows;
    /*! \brief The number of columns of the sparse matrix */
    uint32_t num_cols;
    /*! \brief The non-zero data of the sparse matrix */
    std::vector<data_t> data;
    /*! \brief The column indices of the sparse matrix */
    std::vector<uint32_t> indices;
    /*! \brief The index pointers of the sparse matrix */
    std::vector<uint32_t> indptr;

    // Constructor: initialize an empty CSR matrix
    CSRMatrix(uint32_t num_rows, uint32_t num_cols) {
        this->num_rows = num_rows;
        this->num_cols = num_cols;
        this->indptr.resize(num_rows + 1);
        std::fill(this->indptr.begin(), this->indptr.end(), 0);
    }

    // Default constructor: for resize() functions
    CSRMatrix() {
        this->num_rows = 0;
        this->num_cols = 0;
    }

    // Generate a uniformly distributed CSR matrix
    void uniform_fill(unsigned avg_degree, data_t val) {
        unsigned index_incr = this->num_cols / avg_degree;
        this->data.resize(this->num_rows * avg_degree);
        this->indices.resize(this->num_rows * avg_degree);
        this->indptr.resize(this->num_rows + 1);
        std::fill(this->data.begin(), this->data.end(), val);
        for (size_t i = 0; i < this->num_rows; i++) {
            for (size_t j = 0; j < avg_degree; j++) {
                this->indices[i * avg_degree + j] =
                    ((i % avg_degree) + j * index_incr) % this->num_cols;
            }
        }
        std::generate(
            this->indptr.begin(), this->indptr.end(),
            [=, i = 0U]()mutable{return (i++) * avg_degree;}
        );
    }

    // Generate a randomly distributed CSR matrix
    void random_fill(unsigned avg_degree, unsigned deg_var, data_t val) {
        this->data.reserve(this->num_rows * (avg_degree + deg_var));
        this->indices.reserve(this->num_rows * (avg_degree + deg_var));
        unsigned nnz_cnt = 0;
        for (size_t i = 0; i < this->num_rows; i++) {
            unsigned deg = avg_degree + rand() % (2 * deg_var) - deg_var;
            std::vector<unsigned> index_seq = rand_incr(0, this->num_cols, deg);
            std::copy(index_seq.begin(), index_seq.end(), std::back_inserter(this->indices));
            this->data.resize(this->data.size() + deg, val);
            nnz_cnt += deg;
            this->indptr[i + 1] = nnz_cnt;
        }
    }

    // Generate a dense CSR matrix
    void dense_fill(data_t val) {
        this->data.resize(this->num_rows * this->num_cols);
        this->indices.resize(this->num_rows * this->num_cols);
        std::fill(this->data.begin(), this->data.end(), val);
        std::generate(
            this->indices.begin(), this->indices.end(),
            [=, i = 0U]()mutable{return (i++) % this->num_cols;}
        );
        std::generate(
            this->indptr.begin(), this->indptr.end(),
            [=, i = 0U]()mutable{return (i++) * this->num_cols;}
        );
    }
/***
    // Load a csr matrix from a scipy sparse npz file. The sparse matrix should have float data type.
    void load_from_float_npz(std::string csr_float_npz_path) {
        cnpy::npz_t npz = cnpy::npz_load(csr_float_npz_path);
        cnpy::NpyArray npy_shape = npz["shape"];
        uint32_t num_rows = npy_shape.data<uint32_t>()[0];
        uint32_t num_cols = npy_shape.data<uint32_t>()[2];
        this->num_rows = num_rows;
        this->num_cols = num_cols;
        cnpy::NpyArray npy_data = npz["data"];
        uint32_t nnz = npy_data.shape[0];
        cnpy::NpyArray npy_indices = npz["indices"];
        cnpy::NpyArray npy_indptr = npz["indptr"];

        // copy data to a temporary float vector
        std::vector<float> tmp_data_float;
        tmp_data_float.insert(tmp_data_float.begin(), &npy_data.data<float>()[0],
            &npy_data.data<float>()[nnz]);

        // copy metadata
        this->indices.insert(this->indices.begin(), &npy_indices.data<uint32_t>()[0],
            &npy_indices.data<uint32_t>()[nnz]);
        this->indptr.insert(this->indptr.begin(), &npy_indptr.data<uint32_t>()[0],
            &npy_indptr.data<uint32_t>()[num_rows + 1]);

        // quantize float data
        this->data.resize(tmp_data_float.size());
        for (size_t i = 0; i < this->data.size(); i++) {
            this->data[i] = (data_t)tmp_data_float[i];
        }
    } ***/

    // store dataset into a text file
    void store_to_txt(std::string txt_path) {
        std::ofstream f;
        f.open(txt_path);
        f << "num_rows" << std::endl << this->num_rows << std::endl;
        f << "num_cols" << std::endl << this->num_cols << std::endl;
        f << "data" << std::endl;
        for (auto &&x : this->data) {
            f << x << std::endl;
        }
        f << "indices" << std::endl;
        for (auto &&x : this->indices) {
            f << x << std::endl;
        }
        f << "indptr" << std::endl;
        for (auto &&x : this->indptr) {
            f << x << std::endl;
        }
        f.close();
    }

    // load dataset from a text file
    void load_from_txt(std::string txt_path) {
        std::ifstream f;
        f.open(txt_path);
        std::string line;
        this->data.clear();
        this->indices.clear();
        this->indptr.clear();
        int curr_section = -1; // 0 for Nrows, 1 for Ncols, 2 for data, 3 for indices, 4 for indptr
        while (std::getline(f, line)) {
            if (line.find("num_rows") != std::string::npos) {
                curr_section ++;
                continue;
            } else if (line.find("num_cols") != std::string::npos) {
                curr_section ++;
                continue;
            } else if (line.find("data") != std::string::npos) {
                curr_section ++;
                continue;
            } else if (line.find("indices") != std::string::npos) {
                curr_section ++;
                continue;
            } else if (line.find("indptr") != std::string::npos) {
                curr_section ++;
                continue;
            }

            switch (curr_section) {
            case 0:
                this->num_rows = std::stoi(line);
                break;
            case 1:
                this->num_cols = std::stoi(line);
                break;
            case 2:
                this->data.push_back(static_cast<data_t>(std::stod(line)));
                break;
            case 3:
                this->indices.push_back(std::stoi(line));
                break;
            case 4:
                this->indptr.push_back(std::stoi(line));
                break;
            default:
                break;
            }
        }
        f.close();
    }

    // round dimensions to fit memory alignment by adding empty rows/cols
    void round_dim(unsigned row_divisor, unsigned col_divisor) {
        if (this->num_rows % row_divisor != 0) {
            unsigned new_num_rows = (this->num_rows + row_divisor - 1) / row_divisor * row_divisor;
            unsigned total_nnz = this->indptr[this->num_rows];
            this->indptr.resize(new_num_rows, total_nnz);
            this->num_rows = new_num_rows;
        }
        if (this->num_cols % col_divisor != 0) {
            unsigned new_num_cols = (this->num_cols + col_divisor - 1) / col_divisor * col_divisor;
            this->num_cols = new_num_cols;
        }
    }

};

//-------------------------------------------------------------------
// Variants of CSR for SpMV
//-------------------------------------------------------------------

// a partitioned matrix in CSR
template<typename data_t>
class PartitionedCSR {
public:
    unsigned row_partition_size;
    unsigned col_partition_size;
    unsigned num_row_partitions;
    unsigned num_col_partitions;
    unsigned num_rows_last;
    unsigned num_cols_last;
    unsigned total_num_rows;
    unsigned total_num_cols;
    std::vector<std::vector<CSRMatrix<data_t> > > partitions;

    // Constructor: pass in a CSR and the partition sizes.
    PartitionedCSR (
        CSRMatrix<data_t> &mat,
        const unsigned row_partition_size,
        const unsigned col_partition_size
    ) {
        // initialize partition parameters
        this->total_num_rows = mat.num_rows;
        this->total_num_cols = mat.num_cols;
        this->row_partition_size = row_partition_size;
        this->col_partition_size = col_partition_size;
        this->num_row_partitions = (mat.num_rows + row_partition_size - 1) / row_partition_size;
        this->num_col_partitions = (mat.num_cols + col_partition_size - 1) / col_partition_size;
        this->num_rows_last = (mat.num_rows % row_partition_size) ?
                              (mat.num_rows % row_partition_size) : row_partition_size;
        this->num_cols_last = (mat.num_cols % col_partition_size) ?
                              (mat.num_cols % col_partition_size) : col_partition_size;

        // temporary array for row lengths
        std::vector<std::vector<std::vector<uint32_t> > > row_len;
        row_len.resize(num_row_partitions);
        // initialize partitions
        this->partitions.resize(num_row_partitions);
        for (size_t i = 0; i < num_row_partitions; i++) {
            (this->partitions)[i].resize(num_col_partitions);
            row_len[i].resize(num_col_partitions);
            unsigned nrows = (i == num_row_partitions - 1) ? this->num_rows_last : row_partition_size;
            for (size_t j = 0; j < num_col_partitions; j++) {
                row_len[i][j].resize(nrows + 1);
                std::fill(row_len[i][j].begin(), row_len[i][j].end(), 0);
                unsigned ncols = (j == num_col_partitions - 1) ? this->num_cols_last : col_partition_size;
                // reserve some memory assuming uniform distribution (can be removed)
                (this->partitions)[i][j].data.reserve(
                    mat.data.size() / (num_row_partitions * num_col_partitions)
                );
                (this->partitions)[i][j].indices.reserve(
                    mat.data.size() / (num_row_partitions * num_col_partitions)
                );
                // initialize index pointers
                (this->partitions)[i][j].indptr.resize(nrows + 1);
                // matrix parameters
                (this->partitions)[i][j].num_rows = nrows;
                (this->partitions)[i][j].num_cols = ncols;
            }
        }

        // dispatch non-zeros into partitions
        for (size_t rid = 0; rid < mat.num_rows; rid++) {
            unsigned start = mat.indptr[rid];
            unsigned end = mat.indptr[rid + 1];
            unsigned part_r = rid / row_partition_size;
            unsigned r_offset = rid % row_partition_size;
            for (size_t nzid = start; nzid < end; nzid++) {
                unsigned cid = mat.indices[nzid];
                data_t value = mat.data[nzid];
                unsigned part_c = cid / col_partition_size;
                (this->partitions)[part_r][part_c].indices.push_back(cid);
                (this->partitions)[part_r][part_c].data.push_back(value);
                row_len[part_r][part_c][rid % row_partition_size]++;
            }
        }

        // construct row pointer arrays
        for (size_t i = 0; i < num_row_partitions; i++) {
            for (size_t j = 0; j < num_col_partitions; j++) {
                for (size_t k = 0; k < (this->partitions)[i][j].num_rows; k++) {
                    (this->partitions)[i][j].indptr[k + 1] =
                        (this->partitions)[i][j].indptr[k] + row_len[i][j][k];
                }
            }
        }
    }
};

// data structures for CPSR
template<typename data_t, unsigned pack_size>
struct channel_word_t {
    struct {
        uint32_t index;
        data_t value;
    } packets[pack_size];
};

// CPSR (cyclic packed streams of rows) format. It is always partitioned.
template<typename data_t, unsigned pack_size>
class CPSRMatrix {
public:
    unsigned num_channels;
    unsigned row_partition_size;
    unsigned col_partition_size;
    unsigned num_row_partitions;
    unsigned num_col_partitions;
    unsigned total_num_rows;
    unsigned total_num_cols;
    unsigned num_rows_last;
    unsigned num_cols_last;

    const uint32_t next_row_marker = 0xFFFFFFFF;

    std::vector<std::vector<channel_word_t<data_t, pack_size> > > channel_data;
    std::vector<std::vector<channel_word_t<data_t, pack_size> > > partition_tables;

    // append a CSR matrix to the current channel data
    void append_with_CSR (CSRMatrix<data_t> &mat, bool skip_empty_rows = false) {
        unsigned num_streams = pack_size * this->num_channels;

        // -------- Streamizing --------

        // temporary arrays
        std::vector<std::vector<data_t> > tmp_data(num_streams);
        std::vector<std::vector<uint32_t> > tmp_indices(num_streams);

        // TODO: reserve memory to improve performance
        // tmp_data[strid].reserve(end - start + tmp_data[strid].size());
        // tmp_indices[strid].reserve(end - start + tmp_indices[strid].size());

        // assign one partition to multiple channels, stored in temp arrays
        for (size_t rid = 0; rid < mat.num_rows; rid++) {
            unsigned strid = rid % num_streams;
            unsigned start = mat.indptr[rid];
            unsigned end = mat.indptr[rid + 1];
            // TODO: support empty row skipping
            // if (skip_empty_rows && start == end) {
            // }
            std::copy(mat.data.begin() + start, mat.data.begin() + end, std::back_inserter(tmp_data[strid]));
            std::copy(mat.indices.begin() + start, mat.indices.begin() + end, std::back_inserter(tmp_indices[strid]));
            // add next-row markers
            tmp_data[strid].push_back(1);
            tmp_indices[strid].push_back(this->next_row_marker);
        }

        // add new empty entries to the partition table
        for (size_t i = 0; i < this->num_channels; i++) {
            this->partition_tables[i].resize(this->partition_tables[i].size() + 2);
            // update partition offset in the partition table
            (this->partition_tables)[i][this->partition_tables[i].size() - 2].packets[0].index
                = (this->channel_data)[i].size();
        }

        // update stream lengths in the partition table
        for (size_t i = 0; i < num_streams; i++) {
            unsigned chid = i / pack_size % (this->num_channels);
            unsigned pkid = i % pack_size;
            (this->partition_tables)[chid][this->partition_tables[chid].size() - 1].packets[pkid].index
                = tmp_data[i].size();
        }

        // -------- Packing --------

        // find the longest stream length of each channel
        unsigned max_len[num_channels];
        unsigned insert_offset[num_channels];
        for (size_t i = 0; i < num_channels; i++) {
            max_len[i] = 0;
            for (size_t j = 0; j < pack_size; j++) {
                unsigned strid = j + i * pack_size;
                if (tmp_data[strid].size() > max_len[i])
                    max_len[i] = tmp_data[strid].size();
            }
            // locate the insert point
            insert_offset[i] = (this->channel_data)[i].size();
            // allocate more memory
            (this->channel_data)[i].resize(insert_offset[i] + max_len[i]);
        }

        // append data
        for (size_t i = 0; i < num_streams; i++) {
            // figure out which channel/pack_idx does this stream belong to
            unsigned chid = i / pack_size % (this->num_channels);
            unsigned pkid = i % pack_size;
            // padd zeros to shorter streams
            tmp_data[i].resize(max_len[chid], 0);
            tmp_indices[i].resize(max_len[chid], 0);
            // data copy
            for (size_t j = 0; j < max_len[chid]; j++) {
                (this->channel_data)[chid][insert_offset[chid] + j].packets[pkid].index = tmp_indices[i][j];
                (this->channel_data)[chid][insert_offset[chid] + j].packets[pkid].value = tmp_data[i][j];
            }
        }
    }

    // Constructor: pass in a PartitionedCSR matrix and the number of memory channels
    CPSRMatrix (PartitionedCSR<data_t> &mat, uint32_t num_channels) {
        this->num_channels = num_channels;
        this->total_num_rows = mat.total_num_rows;
        this->total_num_cols = mat.total_num_cols;
        this->row_partition_size = mat.row_partition_size;
        this->col_partition_size = mat.col_partition_size;
        this->num_row_partitions = mat.num_row_partitions;
        this->num_col_partitions = mat.num_col_partitions;
        this->num_rows_last = mat.num_rows_last;
        this->num_cols_last = mat.num_cols_last;

        // check dimensionality
        if (mat.row_partition_size % (num_channels * pack_size) != 0) {
            printf("[ERROR]: the row partition size of %d does not"
                   "divide total stream count of %d\n", mat.row_partition_size, num_channels * pack_size);
            fflush(stdout);
            throw std::invalid_argument("partition size too small");
        }
        // note: it can be 0!
        unsigned nrows_last_part = mat.total_num_rows % mat.row_partition_size;
        if (nrows_last_part > 0 && nrows_last_part % (num_channels * pack_size) != 0) {
            printf("[ERROR]: the size of the last row partition %d does not"
                   "divide total stream count of %d\n", nrows_last_part, (num_channels * pack_size));
            fflush(stdout);
            throw std::invalid_argument("partition size too small");
        }

        // iterate all partitions and append them to channel data
        this->channel_data.resize(num_channels);
        this->partition_tables.resize(num_channels);
        for (size_t i = 0; i < mat.num_row_partitions; i++) {
            for (size_t j = 0; j < mat.num_col_partitions; j++) {
                this->append_with_CSR(mat.partitions[i][j]);
            }
        }
    }
};

// print functions

template<typename data_t>
void print_CSR(CSRMatrix<data_t> &mat) {
    int i = 0;
    printf("Data   : [");
    for (auto &&x : mat.data) {
        if (typeid(data_t) == typeid(float))
            printf("%f, ", x);
        else
            printf("%d, ", x);
        if (i++ > 70) {printf("..."); break;}
    }
    printf("]\n");
    i = 0;
    printf("Indices: [");
    for (auto &&x : mat.indices) {
        printf("%d, ", x);
        if (i++ > 70) {printf("..."); break;}
    }
    printf("]\n");
    i = 0;
    printf("Row pointer: [");
    for (auto &&x : mat.indptr) {
        printf("%d, ", x);
        if (i++ > 70) {printf("..."); break;}
    }
    printf("]\n");
}

template<typename data_t>
void print_PartitionedCSR(PartitionedCSR<data_t> &mat) {
    for (size_t i = 0; i < mat.num_row_partitions; i++) {
        for (size_t j = 0; j < mat.num_col_partitions; j++) {
            printf("---- Partition (%ld, %ld) ----\n", i, j);
            print_CSR(mat.partitions[i][j]);
        }
    }
}

template<typename data_t, unsigned pack_size>
void print_CPSR(CPSRMatrix<data_t, pack_size> &mat) {
    for (size_t i = 0; i < mat.num_channels; i++) {
        printf("---- Partition Table on Channel %ld ----\n", i);
        for (size_t j = 0; j < pack_size; j++) {
            printf("Stream %2ld indices [", j + i * pack_size);
            for (size_t k = 0; k < mat.partition_tables[i].size(); k++) {
                printf("%2d, ", mat.partition_tables[i][k].indices[j]);
                if (k > 70) {printf("..."); break;}

            }
            printf("]\n");
            printf("          values  [");
            for (size_t k = 0; k < mat.partition_tables[i].size(); k++) {
                if (typeid(data_t) == typeid(float))
                    printf("%2f, ", mat.partition_tables[i][k].values[j]);
                else
                    printf("%2d, ", mat.partition_tables[i][k].values[j]);
                if (k > 70) {printf("..."); break;}
            }
            printf("]\n");
        }
    }

    for (size_t i = 0; i < mat.num_channels; i++) {
        printf("---- Data on Channel %ld ----\n", i);
        for (size_t j = 0; j < pack_size; j++) {
            printf("Stream %2ld indices [", j + i * pack_size);
            for (size_t k = 0; k < mat.channel_data[i].size(); k++) {
                if (mat.channel_data[i][k].indices[j] == mat.next_row_marker)
                    printf(" +, ");
                else if (mat.channel_data[i][k].values[j] == 0)
                    printf("  , ");
                else
                    printf("%2d, ", mat.channel_data[i][k].indices[j]);
                if (k > 70) {printf("..."); break;}
            }
            printf("]\n");
            printf("          values  [");
            for (size_t k = 0; k < mat.channel_data[i].size(); k++) {
                if (mat.channel_data[i][k].values[j] == 0)
                    printf("  , ");
                else
                    if (typeid(data_t) == typeid(float))
                        printf("%2f, ", mat.channel_data[i][k].values[j]);
                    else
                        printf("%2d, ", mat.channel_data[i][k].values[j]);
                if (k > 70) {printf("..."); break;}
            }
            printf("]\n");
        }
    }
}

}  // namespace io
}  // namespace spmv


#endif  // LIBSW_SP_DATA_STRUCTURE_H
