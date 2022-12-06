#include <stdio.h>

#include "graph.h"

Graph* createGraph(const std::string &gName, const std::string &mode) {
    Graph* gptr;
    std::string dir = mode;

    if (gName == "AM") {
        gptr = new Graph(dir + "AM/amazon-2008/amazon-2008.mtx");
    }
    else if (gName == "BB") {
        gptr = new Graph(dir + "BB/web-baidu-baike.edges");
    }
    else if (gName == "FU") {
        gptr = new Graph(dir + "FU/soc-flickr-und.edges");
    }
    else if (gName == "G23") {
        gptr = new Graph(dir + "G23/graph500-scale23-ef16_adj/graph500-scale23-ef16_adj.edges");
    }
    else if (gName == "G24") {
        gptr = new Graph(dir + "G24/graph500-scale24-ef16_adj.edges");
    }
    else if (gName == "G25") {
        gptr = new Graph(dir + "G25/graph500-scale25-ef16_adj.edges");
    }
    else if (gName == "GG") {
        gptr = new Graph(dir + "GG/web-Google/web-Google.mtx");
    }
    else if (gName == "HD") {
        gptr = new Graph(dir + "HD/web-hudong.edges");
    }
    else if (gName == "HW") {
        gptr = new Graph(dir + "HW/ca-hollywood-2009.mtx");
    }
    else if (gName == "LJ") {
        gptr = new Graph(dir + "LJ/soc-LiveJournal1.txt");
    }
    else if (gName == "MG") {
        gptr = new Graph(dir + "MG/bio-mouse-gene/bio-mouse-gene.edges");
    }
    else if (gName == "PK") {
        gptr = new Graph(dir + "PK/soc-pokec-relationships.txt");
    }
    else if (gName == "TC") {
        gptr = new Graph(dir + "TC/wiki-topcats.txt");
    }
    else if (gName == "WT") {
        gptr = new Graph(dir + "WT/wiki-Talk/wiki-Talk.edges");
    }
    else if (gName == "R19") {
        gptr = new Graph("/data/graph_dataset/rmat-19-32.txt");
    }
    else {
        gptr = new Graph("/data/graph_dataset/rmat-12-4.txt");
        std::cout << "load /data/graph_dataset/rmat-12-4.txt .." << std::endl;
    }
    return gptr;
}


double getCurrentTimestamp(void) {
    timespec a;
    clock_gettime(CLOCK_MONOTONIC, &a);
    return (double(a.tv_nsec) * 1.0e-9) + double(a.tv_sec);
}

