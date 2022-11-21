#include <hls_stream.h>
#include "graph_fpga.h"


#include "fpga_global_mem.h"
#include "fpga_apply.h"


extern "C" {
    void  readVertex(
        uint16                  *input_from_mem,
        hls::stream<burst_raw>  &input_from_up,
        unsigned int            vertexNum
    )
    {
#pragma HLS INTERFACE m_axi port=input_from_mem offset=slave bundle=gmem%THUNDERGP_GMEM_ID% max_read_burst_length=64 num_write_outstanding=4
#pragma HLS INTERFACE s_axilite port=input_from_mem bundle=control

#pragma HLS INTERFACE s_axilite port=vertexNum      bundle=control
#pragma HLS INTERFACE s_axilite port=return         bundle=control

        burstReadLite(0, vertexNum, input_from_mem, input_from_up);
    }
}