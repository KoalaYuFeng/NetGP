#include "./include/graph.h"
#include "./include/global_config.h"
#include "../common/includes/cmdparser/cmdlineparser.h"
#include <cstdlib>
#include <iostream>
#include <fstream>

void Graph::loadFile(
    const std::string& gName,
    std::vector<std::vector<int>> &data
)
{
    std::ifstream fhandle(gName.c_str());
    if (!fhandle.is_open()) {
        HERE;
        std::cout << "Failed to open " << gName << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string line;
    while (std::getline(fhandle, line)) {
        std::istringstream iss(line);
        data.push_back(
            std::vector<int>(std::istream_iterator<int>(iss),
                             std::istream_iterator<int>())
        );
    }
    fhandle.close();

    std::cout << "Graph " << gName << " is loaded." << std::endl;
}

int Graph::getMaxIdx(const std::vector<std::vector<int>> &data) {
    int maxIdx = data[0][0];
    for (auto it1 = data.begin(); it1 != data.end(); it1++) {
        for (auto it2 = it1->begin(); it2 != it1->end(); it2++) {
            if (maxIdx <= (*it2)) {
                maxIdx = *it2;
            }
        }
    }
    return maxIdx;
}

int Graph::getMinIdx(const std::vector<std::vector<int>> &data) {
    int minIdx = data[0][0];
    for (auto it1 = data.begin(); it1 != data.end(); it1++) {
        for (auto it2 = it1->begin(); it2 != it1->end(); it2++) {
            if (minIdx >= (*it2)) {
                minIdx = *it2;
            }
        }
    }
    return minIdx;
}

Graph::Graph(const std::string& gName) {

    // Check if it is undirectional graph
    auto found = gName.find("ungraph", 0);
    if (found != std::string::npos)
        isUgraph = true;
    else
        isUgraph = false;

    std::vector<std::vector<int>> data;
    loadFile(gName, data);
    vertexNum = getMaxIdx(data) + 1;
    edgeNum = (int)data.size();
    std::cout << "vertex num: " << vertexNum << std::endl;
    std::cout << "edge num: " << edgeNum << std::endl;

    for (int i = 0; i < vertexNum; i++) {
        Vertex* v = new Vertex(i);
        vertices.push_back(v);
    }

    for (auto it = data.begin(); it != data.end(); it++) {
        int srcIdx = (*it)[0];
        int dstIdx = (*it)[1];
        vertices[srcIdx]->outVid.push_back(dstIdx);
        vertices[dstIdx]->inVid.push_back(srcIdx);
        if (isUgraph && srcIdx != dstIdx) {
            vertices[dstIdx]->outVid.push_back(srcIdx);
            vertices[srcIdx]->inVid.push_back(dstIdx);
        }
    }

    for (auto it = vertices.begin(); it != vertices.end(); it++) {
        (*it)->inDeg = (int)(*it)->inVid.size();
        (*it)->outDeg = (int)(*it)->outVid.size();
    }
}

CSR::CSR(const Graph &g) : vertexNum(g.vertexNum), edgeNum(g.edgeNum) {
    rpao.resize(vertexNum + 1);
    rpai.resize(vertexNum + 1);
    rpao[0] = 0;
    rpai[0] = 0;
    for (int i = 0; i < vertexNum; i++) {
        rpao[i + 1] = rpao[i] + g.vertices[i]->outDeg;
        rpai[i + 1] = rpai[i] + g.vertices[i]->inDeg;
    }

    // sort the input and output vertex
    for (int i = 0; i < vertexNum; i++) {
        std::sort(g.vertices[i]->outVid.begin(), g.vertices[i]->outVid.end());
        std::sort(g.vertices[i]->inVid.begin(), g.vertices[i]->inVid.end());
        for (auto id : g.vertices[i]->outVid) {
            ciao.push_back(id);
        }
        for (auto id : g.vertices[i]->inVid) {
            ciai.push_back(id);
        }
    }
#if 0
    for (int i = 0; i < edgeNum; i++) {
        eProps.push_back(rand() % 10);
    }
#endif
}

int CSR::save2BinaryFile(const std::string &fName)
{
    std::cout << "start writing to binary file" <<std::endl;
    std::ostringstream command;
    command << "mkdir -p ";
    command <<"csr/";
    command << fName;
    int ret = system(command.str().c_str());
    if (ret < 0)
    {
        return ret;
    }


    std::string name;
    // write rpao to binary.
    name = "csr/" + fName + "/" + fName + "_rapo.bin";
    std::ofstream fFileWrite_rapo(name, std::ios::binary);
	if(!fFileWrite_rapo)
	{
		std::cout << "create file failed! " << std::endl;
		return -1;
	}

    int rpao_size = rpao.size();
    std::cout << "rpao size = "<< rpao_size << std::endl;
    fFileWrite_rapo.write((char*)&rpao_size, sizeof(int));
	fFileWrite_rapo.write((char*)&rpao[0], sizeof(int) * (rpao.size()));
	fFileWrite_rapo.close();

    // write rpai to binary.
    name = "csr/" + fName + "/" + fName + "_rapi.bin";
    std::ofstream fFileWrite_rapi(name, std::ios::binary);
	if(!fFileWrite_rapi)
	{
		std::cout << "create file failed! " << std::endl;
		return -1;
	}

    int rpai_size = rpai.size();
    std::cout << "rpai size = "<< rpai_size << std::endl;
    fFileWrite_rapi.write((char*)&rpai_size, sizeof(int));
	fFileWrite_rapi.write((char*)&rpai[0], sizeof(int) * (rpai.size()));
	fFileWrite_rapi.close();
    //

    // write ciai to binary.
    name = "csr/" + fName + "/" + fName + "_ciai.bin";
    std::ofstream fFileWrite_ciai(name, std::ios::binary);
	if(!fFileWrite_ciai)
	{
		std::cout << "create file failed! " << std::endl;
		return -1;
	}

    int ciai_size = ciai.size();
    std::cout << "ciai size = "<< ciai_size << std::endl;
    fFileWrite_ciai.write((char*)&ciai_size, sizeof(int));
	fFileWrite_ciai.write((char*)&ciai[0], sizeof(int) * (ciai.size()));
	fFileWrite_ciai.close();

    std::cout << "write binary done" << std::endl;

    // read from binary. rapi
    /*
    int read_size= 0;
    name = fName + "_rapi.bin";
    std::ifstream fFileReader(name, std::ios::binary);
	if (!fFileReader)
	{
		std::cout << "read file failed" << std::endl;
		return -1;
	}
    read_size = 0;
    fFileReader.read((char*)&read_size, sizeof(int));
    std::cout << " read file size : "<< read_size << std::endl;

    int* array = new int[read_size];
	fFileReader.read((char*)array, read_size * sizeof(int));
	for (int i=0; i < read_size; i++)
	{
		std::cout << array[i] << std::endl;
	}
	fFileReader.close();
    delete []array;
    */

   /* // write to txt

    std::ostringstream offsetsName, columnsName;
    offsetsName << "csr/";
    offsetsName << fName;
    offsetsName << "/rapo.txt";
    std::ofstream offset;
    offset.open(offsetsName.str().c_str());

    columnsName << "csr/";
    columnsName << fName;
    columnsName << "/ciai.txt";
    std::ofstream columns;
    columns.open(columnsName.str().c_str());

    offset << (rpao.size() - 1) << std::endl;
    for (auto item : rpao)
    {
        offset << item << std::endl;
    }
    offset.flush();
    offset.close();

    columns << ciai.size() << std::endl;
    for (auto item : ciai)
    {
        columns << item << std::endl;
    }
    columns.flush();
    columns.close();
   */
    return 0;
}


Graph* createGraph(const std::string &gName, const std::string &mode) {
    Graph* gptr;
    std::string dir = mode;

    if (gName == "AM") {
        gptr = new Graph(dir + "amazon-2008.mtx");
    }
    else if (gName == "BB") {
        gptr = new Graph(dir + "web-baidu-baike.edges");
    }
    else if (gName == "FU") {
        gptr = new Graph(dir + "soc-flickr.ungraph.edges");
    }
    else if (gName == "G23") {
        gptr = new Graph(dir + "graph500-scale23-ef16_adj.edges");
    }
    else if (gName == "G24") {
        gptr = new Graph(dir + "graph500-scale24-ef16_adj.edges");
    }
    else if (gName == "G25") {
        gptr = new Graph(dir + "graph500-scale25-ef16_adj.edges");
    }
    else if (gName == "GG") {
        gptr = new Graph(dir + "web-Google.mtx");
    }
    else if (gName == "HD") {
        gptr = new Graph(dir + "web-hudong.edges");
    }
    else if (gName == "HW") {
        gptr = new Graph(dir + "ca-hollywood-2009.mtx");
    }
    else if (gName == "LJ") {
        gptr = new Graph(dir + "LiveJournal1.txt");
    }
    else if (gName == "MG") {
        gptr = new Graph(dir + "bio-mouse-gene.edges");
    }
    else if (gName == "PK") {
        gptr = new Graph(dir + "pokec-relationships.txt");
    }
    else if (gName == "TC") {
        gptr = new Graph(dir + "wiki-topcats.mtx");
    }
    else if (gName == "WT") {
        gptr = new Graph(dir + "wiki-Talk.txt");
    }
    else if (gName == "R19") {
        gptr = new Graph(dir + "rmat-19-32.txt");
    }
    else if (gName == "R21") {
        gptr = new Graph(dir + "rmat-21-32.txt");
    }
    else if (gName == "R24") {
        gptr = new Graph(dir + "rmat-24-16.txt");
    }
    else if (gName == "TW") {
        gptr = new Graph(dir + "soc-twitter-2010.mtx");
    }
    else if (gName == "WP") {
        gptr = new Graph(dir + "wikipedia-20070206.mtx");
    }
    else {
        gptr = new Graph("/data/graph_dataset/rmat-12-4.txt");
        std::cout << "load /data/graph_dataset/rmat-12-4.txt as default ..." << std::endl;
    }
    return gptr;
}

int main (int argc, char **argv) {

    // Read settings
    if (argc < 2) {
        std::cout << "please type ./graph HW(dataset name) " << std::endl;
    }
    std::string g_name = argv[1];
    std::string path_graph_dataset = "/data/graph_dataset/";
    Graph* gptr = createGraph(g_name, path_graph_dataset);
    CSR* csr    = new CSR(*gptr);
    free(gptr);
    csr->save2BinaryFile(g_name);
    free(csr);
}
