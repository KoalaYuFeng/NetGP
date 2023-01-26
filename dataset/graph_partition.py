import numpy as np
import os

dataset = [['R19', 'rmat-19-32.txt'], ['R21','rmat-21-32.txt'], ['R24','rmat-24-16.txt'], ['G23', 'graph500-scale23-ef16_adj.edges'],
           ['G24', 'graph500-scale24-ef16_adj.edges'], ['G25', 'graph500-scale25-ef16_adj.edges'], ['WP', 'wikipedia-20070206.mtx'], 
           ['HW', 'ca-hollywood-2009.mtx'], ['LJ', 'LiveJournal1.txt'], ['TW', 'soc-twitter-2010.mtx'], ['R12', 'rmat-12-4.txt']]
print (dataset)

for dataset_index in range(len(dataset)):
    
    parent_path = './graph_dataset/'
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
    UNMAPPED_FLAG = 4294967295 ## 0xffffffff, max vertex index
    END_FLAG = 4294967294 ## 0xffffffff - 1, end_flag value
    alignment_size = 4*1024 ## edge num of each sub-partition should be 4K align

    print ("Load data from hard-disk ... ... ", filename)
    txt_array_t = np.int64(np.loadtxt(filename))
    txt_array = txt_array_t[:,:2]

    max_index = np.int32(txt_array.max())
    print ("Vertex index range : 0 -", max_index)
    print ("Before compress, edge number ", np.int32(len(txt_array)))

    ## delete vertices whose out degree equals to 0;
    src_array = txt_array[:, 0]

    max_index_temp = src_array.max()
    min_index_temp = src_array.min()
    ## print(max_index_temp)
    ## print(min_index_temp)

    outdegree = np.zeros(max_index + 1, dtype = np.int32)
    outdegree[ : max_index_temp+1] = np.bincount(src_array) ## start from 0 to max+1
    np.savetxt(path + "/outDeg.txt", outdegree, fmt='%d', delimiter=' ')
    ## print(outdegree)

    mapping_index = np.zeros(max_index + 1, dtype = np.uint32) ## compressed vertex index to original index
    tmp_index = 0
    for i in range(len(mapping_index)):
        if (outdegree[i] == 0):
            mapping_index[i] = UNMAPPED_FLAG ## 0xFFFFFFFF means invalid vertex
        else:
            mapping_index[i] = tmp_index
            tmp_index += 1
    np.savetxt(path + "/index_mapping.txt", mapping_index, fmt='%d', delimiter=' ')
    print ("After compress, vertex number ", tmp_index + 1)

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
        if ((mapping_index[txt_array[i][0]] == UNMAPPED_FLAG) | (mapping_index[txt_array[i][1]] == UNMAPPED_FLAG)):
            print("Wrong index in vertex compression")
            exit(-1)
        else:
            txt_array[i][0] = mapping_index[txt_array[i][0]]
            txt_array[i][1] = mapping_index[txt_array[i][1]]

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
        edge_num_p = edge_num_p/sub_partition_num
        edge_num_p = np.int32((edge_num_p + alignment_size - 1)/alignment_size) * alignment_size

        for j in range(sub_partition_num):
            array_sp = np.zeros((edge_num_p, 2),dtype = np.uint32)
            for k in range(edge_num_p):
                if ((j*edge_num_p + k) >= len(array_temp)):
                    array_sp[k][0] = END_FLAG
                    array_sp[k][1] = array_temp[len(array_temp)-1][1]
                else:    
                    array_sp[k, :] = array_temp[j*edge_num_p + k, :]
            
            ## sort sub-partition array
            array_sp = array_sp[np.lexsort([array_sp.T[0]])]
            name = path + "/p_" + str(i) + "_sp_" + str(j) + ".txt"
            np.savetxt(name, array_sp, fmt='%d', delimiter=' ')

    print ("Save dataset done : ", directory)
