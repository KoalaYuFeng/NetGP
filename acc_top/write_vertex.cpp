#include <hls_stream.h>
#include "graph_fpga.h"


#include "fpga_global_mem.h"
#include "fpga_apply.h"


extern "C" {
    void  writeVertex(
        uint16                  *output_to_mem,
        hls::stream<burst_dest>  &output_to_net,
        unsigned int            vertexNum
    )
    {
#pragma HLS INTERFACE m_axi port=output_to_mem offset=slave bundle=gmem2 \
                        max_read_burst_length=64 num_read_outstanding=32 \
                        max_write_burst_length=64 num_write_outstanding=32

#pragma HLS INTERFACE s_axilite port=output_to_mem bundle=control

#pragma HLS INTERFACE s_axilite port=vertexNum      bundle=control
#pragma HLS INTERFACE s_axilite port=return         bundle=control

        writeBackLite(vertexNum, output_to_mem, output_to_net);
    }
}