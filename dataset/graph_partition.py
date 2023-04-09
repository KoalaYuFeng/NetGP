import numpy as np
import os

dataset = [['R12', 'rmat-12-4.txt'], ['WP', 'wikipedia-20070206.mtx'], ['HW', 'ca-hollywood-2009.mtx'], ['LJ', 'LiveJournal1.txt'], 
           ['R19', 'rmat-19-32.txt'], ['R21','rmat-21-32.txt'], ['R24','rmat-24-16.txt'], ['G23', 'graph500-scale23-ef16_adj.edges'],
           ['G24', 'graph500-scale24-ef16_adj.edges'], ['G25', 'graph500-scale25-ef16_adj.edges'], ['TW', 'twitter-2010.txt'],
           ['K26', 'kron-26-16.txt'], ['K28', 'kron-28-16.txt']]
print (dataset)

for dataset_index in range(len(dataset)):
    
    parent_path = '/data/yufeng/large_graph/'
    if (os.path.exists(parent_path) == True):
        print("Already exists ", parent_path)
    else :
        os.mkdir(parent_path)
        print("Create directory ", parent_path)

    directory = dataset[dataset_index][0]
    path = os.path.join(parent_path, directory)

    if (os.path.exists(path) == True):
        print("Already exists ", path)
        continue
    else :
        os.mkdir(path)
        print("Create directory ", path)

    filename = '/data/graph_dataset/' + dataset[dataset_index][1]
    ## filename = '/data/graph_dataset/rmat-19-32.txt' ## take R19 for example
    ## filename = '/data/graph_dataset/rmat-12-4.txt'
    partition_size = 1024*1024 ## can be 1024*1024 or 512*1024
    sub_partition_num = 12
    UNMAPPED_FLAG = -1 ## outDeg = 0
    END_FLAG = -2 ## end_flag value
    alignment_size = 16 ## edge num of each sub-partition should be 16 edge-aligned
    shuffle = True ## subpartition data shuffle for workload balance

    print ("=======================================")
    print ("============= configuration ===========")
    print ("partition size = ", partition_size)
    print ("subpartition num = ", sub_partition_num)
    print ("edge alignment size = ", alignment_size)
    print ("shuffle = ", shuffle)
    print ("load filename = ", filename)
    print ("save file to = ", path)
    print ("=======================================")

    print ("Load data from hard-disk ... ... ", filename)
    txt_array_t = np.int64(np.loadtxt(filename))
    txt_array = txt_array_t[:,:2]

    txt_array_t = 0 ## release array

    max_index = np.int32(txt_array.max())
    print ("Vertex index range : 0 -", max_index)
    print ("Before compress, edge number ", np.int32(len(txt_array)))

    ## delete vertices whose out degree equals to 0;
    src_array = txt_array[:, 0]

    max_index_temp = src_array.max()
    min_index_temp = src_array.min()
    ## print(max_index_temp)
    ## print(min_index_temp)


###### outdeg format: first number is the number of outdeg vertex number, then follows outdeg value.
###### mapping_index format: first number is the number of original vertex number,
###### next is the compressed vertex number, then follows mapping_index value.

    outdegree = np.zeros(max_index + 1, dtype = np.int32)
    outdegree[ : max_index_temp+1] = np.bincount(src_array) ## start from 0 to max+1
    ## print(outdegree)

    mapping_index = np.zeros(max_index + 3, dtype = np.int32) ## compressed vertex index to original index
    tmp_index = 0
    list_out_deg = [0]
    for i in range(len(outdegree)):
        if (outdegree[i] == 0):
            mapping_index[i+2] = UNMAPPED_FLAG
        else:
            mapping_index[i+2] = tmp_index
            list_out_deg.append(outdegree[i])
            tmp_index += 1
    list_out_deg[0] = tmp_index      ## compressed vertex number 
    outDeg = np.array(list_out_deg, dtype = np.int32)
    outDeg.tofile(path + "/outDeg.bin")
    np.savetxt(path + "/outDeg.txt", outDeg, fmt='%d', delimiter=' ')

    mapping_index[0] = max_index + 1 ## original vertex number
    mapping_index[1] = tmp_index     ## compressed vertex number 
    np.savetxt(path + "/idxMap.txt", mapping_index, fmt='%d', delimiter=' ')
    mapping_index.tofile(path + "/idxMap.bin")
    print ("After compress, vertex number ", tmp_index)

    delete_array = np.where(outdegree == 0)
    ## print(delete_array)
    delete_index = np.in1d(txt_array[:, 1], delete_array)
    delete_index = np.where(delete_index == True)
    ## print(delete_index)
    txt_array = np.delete(txt_array, delete_index, axis = 0)
    ## print(txt_array)
    print ("After compress, edge number ", len(txt_array))

    ## partition function
    for i in range(len(txt_array)):
        if ((mapping_index[txt_array[i][0] + 2] == UNMAPPED_FLAG) | (mapping_index[txt_array[i][1] + 2] == UNMAPPED_FLAG)):
            print("Wrong index in vertex compression")
            exit(-1)
        else:
            txt_array[i][0] = mapping_index[txt_array[i][0] + 2]
            txt_array[i][1] = mapping_index[txt_array[i][1] + 2]

    txt_array = txt_array[np.lexsort([txt_array.T[1]])] ## sort array by incremental order
    array_after_ption = [0]
    ## print (txt_array[:, 1])

    index_t = partition_size
    for i in range(len(txt_array) - 1):
        if ((txt_array[i][1] < index_t) & (txt_array[i+1][1] >= index_t)):
            index_t += partition_size
            array_after_ption.append(i+1)

    array_after_ption.append(len(txt_array))
    print("Array after partition ", array_after_ption)

    for i in range(len(array_after_ption) - 1):
        ## print (txt_array[array_after_ption[i]:array_after_ption[i+1],:])
        ## print (txt_array[array_after_ption[i]:array_after_ption[i+1],:].shape)
        array_temp = txt_array[array_after_ption[i]:array_after_ption[i+1],:] ## each partition array
        ## sort partition array
        array_temp = array_temp[np.lexsort([array_temp.T[0]])]

        edge_num_p = len(array_temp)
        edge_num_p = (edge_num_p + sub_partition_num - 1)/sub_partition_num + 1
        edge_num_p = np.int32((edge_num_p + alignment_size - 1)/alignment_size) * alignment_size

        if (shuffle == True):
            ## subpartition using shuffle
            print ("shuffle = True")
            for j in range(sub_partition_num):
                array_sp_shfe = np.zeros((edge_num_p + 1, 2),dtype = np.int32)
                for k in range(edge_num_p):
                    if ((k*sub_partition_num + j) >= len(array_temp)):
                        array_sp_shfe[k+1][0] = array_temp[len(array_temp)-1][0]
                        array_sp_shfe[k+1][1] = END_FLAG
                    else:    
                        array_sp_shfe[k+1, :] = array_temp[k*sub_partition_num + j, :]
                
                ## sort sub-partition array
                array_sp_shfe = array_sp_shfe[np.lexsort([array_sp_shfe.T[0]])]
                array_sp_shfe[0][0] = edge_num_p
                array_sp_shfe[0][1] = edge_num_p ## reserved, can be used for other
                name = path + "/p_" + str(i) + "_sp_" + str(j) + ".txt"
                np.savetxt(name, array_sp_shfe, fmt='%d', delimiter=' ')
                bin_name = path + "/p_" + str(i) + "_sp_" + str(j) + ".bin"
                array_sp_shfe.tofile(bin_name)

        else:
            ## no shuffle
            print ("shuffle = False")
            edge_idx_offset = 0
            edge_idx_t = 0
            for j in range(sub_partition_num):
                array_sp = np.zeros((edge_num_p + 1, 2), dtype = np.int32)
                array_sp[0][0] = 0
                array_sp[0][1] = 0
                for k in range(edge_num_p):
                    if ((k*sub_partition_num + j) >= len(array_temp)):
                        ## corner case: if len(array_temp) can be divided by 16, it will not tough this.
                        ## solution: let edge_num = edge_num + 1, to avoid this.
                        edge_idx_offset = edge_idx_t
                        array_sp[k+1][0] = array_temp[edge_idx_offset - 1][0]
                        array_sp[k+1][1] = END_FLAG
                    else:
                        array_sp[k+1, :] = array_temp[edge_idx_offset + k, :]
                        edge_idx_t += 1
                
                ## sort sub-partition array
                array_sp = array_sp[np.lexsort([array_sp.T[0]])]
                array_sp[0][0] = edge_num_p
                array_sp[0][1] = edge_num_p ## reserved, can be used for other
                name = path + "/p_" + str(i) + "_sp_" + str(j) + ".txt"
                np.savetxt(name, array_sp, fmt='%d', delimiter=' ')
                bin_name = path + "/p_" + str(i) + "_sp_" + str(j) + ".bin"
                array_sp.tofile(bin_name)

    print ("Save dataset done : ", directory)
    print ("\n")
