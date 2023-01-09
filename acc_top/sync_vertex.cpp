#include <hls_stream.h>
#include "graph_fpga.h"

extern "C" {
    void syncVertex(
        hls::stream<burst_dest>&  input,  // Internal Stream
        hls::stream<burst_dest>&  output, // Internal Stream
        int  FIFO_length, // can be configured by host
        int  vertexNum
        ) {
#pragma HLS INTERFACE s_axilite port=FIFO_length    bundle=control
#pragma HLS INTERFACE s_axilite port=vertexNum      bundle=control
#pragma HLS INTERFACE s_axilite port=return         bundle=control
sync:
    int read_count = 0;
    int write_count = 0;
    burst_dest merge_vertex;
    burst_dest forward_vertex;
    while ((write_count < vertexNum) || (read_count < vertexNum)) {

        // output to merge kernel
        if (((write_count - read_count) < FIFO_length) && (write_count < vertexNum)) {
            merge_vertex.data = 0;
            output.write(merge_vertex);
            write_count ++;
        }

        if ((!input.empty()) && (read_count < vertexNum)) {
            input.read(forward_vertex);
            read_count ++;
        }

    }

}
}