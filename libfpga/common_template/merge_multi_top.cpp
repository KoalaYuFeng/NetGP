#include <hls_stream.h>
#include "graph_fpga.h"


#include "fpga_global_mem.h"
#include "fpga_apply.h"



extern "C" {
    void  streamMerge(
        hls::stream<burst_raw>  &input_a,
        hls::stream<burst_raw>  &input_b,
        hls::stream<burst_raw>  &output,
        unsigned int            vertexNum
    )
    {

#pragma HLS INTERFACE s_axilite port=vertexNum      bundle=control
#pragma HLS INTERFACE s_axilite port=return         bundle=control

        int loopNum = (vertexNum >> 4) ;

        for (int i = 0; i < loopNum ; i++)
        {
#pragma HLS PIPELINE II=1
            burst_raw  unit[2];
#pragma HLS ARRAY_PARTITION variable=unit dim=0 complete

            read_from_stream(input_a, unit[0]);
            read_from_stream(input_b, unit[1]);

            burst_raw res;
            for (int inner = 0; inner < 16 ; inner ++)
            {
#pragma HLS UNROLL
                res.range(31 + inner * 32, 0 + inner * 32) 
                    = PROP_COMPUTE_STAGE4 ( unit[0].range(31 + inner * 32, 0 + inner * 32),
                                            unit[1].range(31 + inner * 32, 0 + inner * 32) );
            }
            write_to_stream(output, res);
        }
    }

}