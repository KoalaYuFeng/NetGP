import numpy as np
import os

## load partitioned txt file.
dataset_name = 'G23'

partition_num = 0
subpartition_num = 12
for i in range(100):
    filename = '/data/yufeng/graph_partition/graph_dataset_sub_12/' + dataset_name + '/p_' + str(i) + '_sp_' + str(0) + '.txt'
    if (os.path.exists(filename) == True):
        partition_num = i + 1

parameter_array = np.zeros((partition_num * subpartition_num, 4), dtype=np.int32)
print (parameter_array)

for p in range (partition_num):
    for sp in range(subpartition_num):
        filename = '/data/yufeng/graph_partition/graph_dataset_sub_12/' + dataset_name + '/p_' + str(p) + '_sp_' + str(sp) + '.txt'
        if (os.path.exists(filename) == False):
            exit(-1)
        print('read file : ' + filename)
        txt_array_t = np.int64(np.loadtxt(filename))
        txt_array = txt_array_t[:,:2]
        print (txt_array)
        print (len(txt_array))
        src_idx_range = txt_array[len(txt_array) - 1][0] - txt_array[0][0]
        print(src_idx_range)
        invalid_edge_num = np.count_nonzero(txt_array == 2147483646)
        print(invalid_edge_num)
        parameter_array[p*subpartition_num + sp, 0] = len(txt_array)
        parameter_array[p*subpartition_num + sp, 1] = src_idx_range
        parameter_array[p*subpartition_num + sp, 2] = invalid_edge_num

np.savetxt(dataset_name + '_parameter.txt', parameter_array, fmt='%d', delimiter=' ')






