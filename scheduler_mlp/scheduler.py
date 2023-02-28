import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
import os

## define MLP model parameter
scale_edge = 1024*1024*20
scale_vertex = 1024*1024
scale_time = 1000*500
hidden_num = 100
save_name = './scheduler_mlp.pth'

class MLP(nn.Module):
    def __init__(self, input_size, hidden_size, output_size):
        super(MLP, self).__init__()
        self.fc1 = nn.Linear(input_size, hidden_size, bias = True)
        self.fc2 = nn.Linear(hidden_size, output_size, bias = True)

    def forward(self, x):
        x = self.fc1(x)
        x = self.fc2(x)
        return x

## load partitioned txt file.
dataset_name = 'TW'
subpartition_num = 12
filename = './' + dataset_name + '_parameter.txt'
data_array = np.float32(np.loadtxt(filename))
partition_num = np.int32(len(data_array) / subpartition_num)
data_array[:, 0:1] = data_array[:, 0:1]/scale_edge
data_array[:, 1:2] = data_array[:, 1:2]/scale_vertex
print('read dataset {}, partition = {}, subpartition = {}'.format(filename, partition_num, subpartition_num))

cost_model = MLP(2, hidden_num, 1)
cost_model.load_state_dict(torch.load(save_name))
print('cost model load done!')

with torch.no_grad():
    inputs = torch.from_numpy(data_array[:, :2])
    outputs = cost_model(inputs)
    result_pred = np.int32(outputs.numpy() * scale_time)
    print('get the predicted time for each subpartition ')
    print(result_pred.T)


print('start scheduling ... ')
schedule_list = np.zeros((subpartition_num, partition_num+1), dtype = np.int32)

for p in range(partition_num):
    temp_p = np.arange(subpartition_num)
    temp_p = temp_p[np.argsort(result_pred[subpartition_num*p : subpartition_num*(p+1), 0].T)]
    schedule_list[:, p+1] = temp_p.T
    temp_s = result_pred[subpartition_num*p : subpartition_num*(p+1), 0]
    schedule_list[:, 0] += temp_s.T[temp_p]
    schedule_list = schedule_list[np.argsort(-1*schedule_list[:, 0].T), :]
    ## print(schedule_list)


print('Final estimate time ')
estimate_time = schedule_list[:, 0].T
print(estimate_time)

print('Final execution order ')
exe_order = schedule_list[:, 1:].T
print(exe_order)
np.savetxt(dataset_name + '_order.txt', exe_order, fmt='%d', delimiter=' ')
np.savetxt('/data/yufeng/graph_partition/graph_dataset_sub_12/' + dataset_name + '_order.txt', exe_order, fmt='%d', delimiter=' ')

