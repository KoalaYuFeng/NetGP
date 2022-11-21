#include <hls_stream.h>
#include "graph_fpga.h"


#include "fpga_global_mem.h"
#include "fpga_apply.h"



extern "C" {
    void  streamForward(
        hls::stream<burst_raw>  &input, // the upstream maybe apply kernel or network kernel.
        hls::stream<burst_raw>  &output_a,
        hls::stream<burst_raw>  &output_b,
        unsigned int            vertexNum
    )
    {
// #pragma HLS INTERFACE m_axi port=networkVertexProp offset=slave bundle=gmem0 max_read_burst_length=64 num_write_outstanding=4
// #pragma HLS INTERFACE s_axilite port=networkVertexProp bundle=control

// #pragma HLS INTERFACE m_axi port=newVertexProp%THUNDERGP_VERTEXPROP_ID% offset=slave bundle=gmem%THUNDERGP_GMEM_ID% max_read_burst_length=64 num_write_outstanding=4
// #pragma HLS INTERFACE s_axilite port=newVertexProp%THUNDERGP_VERTEXPROP_ID% bundle=control

#pragma HLS INTERFACE s_axilite port=vertexNum      bundle=control
#pragma HLS INTERFACE s_axilite port=return         bundle=control  

        int loopNum = (vertexNum >> 4) ;

        for (int i = 0; i < loopNum ; i++)
        {
#pragma HLS PIPELINE II=1
            burst_raw  unit;
            read_from_stream(input, unit);
            write_to_stream(output_a, unit);
            write_to_stream(output_b, unit);
        }
    }

}