#include <hls_stream.h>
#include "graph_fpga.h"


#include "fpga_global_mem.h"
#include "fpga_apply.h"



extern "C" {
    void  streamForward(
        hls::stream<burst_dest>  &input, // the upstream maybe apply kernel or network kernel.
        hls::stream<burst_dest>  &output_a,
        hls::stream<burst_dest>  &output_b,
        unsigned int            dest,
        unsigned int            vertexNum
    )
    {
// #pragma HLS INTERFACE m_axi port=networkVertexProp offset=slave bundle=gmem0 max_read_burst_length=64 num_write_outstanding=4
// #pragma HLS INTERFACE s_axilite port=networkVertexProp bundle=control

// #pragma HLS INTERFACE m_axi port=newVertexProp%THUNDERGP_VERTEXPROP_ID% offset=slave bundle=gmem%THUNDERGP_GMEM_ID% max_read_burst_length=64 num_write_outstanding=4
// #pragma HLS INTERFACE s_axilite port=newVertexProp%THUNDERGP_VERTEXPROP_ID% bundle=control

#pragma HLS INTERFACE s_axilite port=vertexNum      bundle=control
#pragma HLS INTERFACE s_axilite port=dest           bundle=control
#pragma HLS INTERFACE s_axilite port=return         bundle=control  

        int loopNum = (vertexNum >> 4) ;

        for (int i = 0; i < loopNum ; i++)
        {
#pragma HLS PIPELINE II=1
            burst_dest  unit;
            read_from_stream(input, unit);
            unit.dest = dest;
            unit.keep = -1;

            if (((loopNum - 1) == i) || ((((i + 1)*64)% 1408) == 0))
                unit.last = 1;
            else
                unit.last = 0;

            write_to_stream(output_a, unit);
            write_to_stream(output_b, unit);
        }
    }

}