import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np

scale_edge = 1024*1024*20
scale_vertex = 1024*1024
scale_time = 1000*500
epoch_num = 20000
learn_rate = 0.005
hidden_num = 100

train_name = './train.txt'
test_name = './test.txt'
save_name = './scheduler_mlp.pth'

# Define the MLP architecture
class MLP(nn.Module):
    def __init__(self, input_size, hidden_size, output_size):
        super(MLP, self).__init__()
        self.fc1 = nn.Linear(input_size, hidden_size, bias = True)
        self.fc2 = nn.Linear(hidden_size, output_size, bias = True)

    def forward(self, x):
        x = self.fc1(x)
        x = self.fc2(x)
        return x

# Define the dataset, load train dataset
train_dataset = np.float32(np.loadtxt(train_name))
train_dataset[:, 0:1] = train_dataset[:, 0:1]/scale_edge
train_dataset[:, 1:2] = train_dataset[:, 1:2]/scale_vertex
train_dataset[:, 3:] = train_dataset[:, 3:]/scale_time
input_dataset = train_dataset[:, :2]
## print(input_dataset)
output_dataset = train_dataset[:, 3:]
## print(output_dataset)
inputs = torch.from_numpy(input_dataset)
targets = torch.from_numpy(output_dataset)

# Define the model, loss function, and optimizer
model = MLP(2, hidden_num, 1)
criterion = nn.MSELoss()
optimizer = optim.SGD(model.parameters(), lr=learn_rate)

# Train the model
for epoch in range(epoch_num):
    optimizer.zero_grad()
    outputs = model(inputs)
    loss = criterion(outputs, targets)
    loss.backward()
    optimizer.step()
    if epoch % 100 == 0:
        print('Epoch [{}/{}], Loss: {:.8f}'.format(epoch + 1, epoch_num, loss.item()))

torch.save(model.state_dict(), save_name)
# Test the model
with torch.no_grad():
    test_dataset = np.float32(np.loadtxt(test_name))
    test_dataset[:, 0:1] = test_dataset[:, 0:1]/scale_edge
    test_dataset[:, 1:2] = test_dataset[:, 1:2]/scale_vertex
    pred = test_dataset[:, :2]
    result = test_dataset[:, 3:]

    inputs = torch.from_numpy(pred)
    model_pred = MLP(2, 100, 1)
    model_pred.load_state_dict(torch.load(save_name))
    outputs = model_pred(inputs)
    result_pred = outputs.numpy()

total_error = 0
for i in range(len(result_pred)):
    print(result_pred[i] * scale_time, end = ' ')
    print(result[i], end = ' ')
    print(result[i] - result_pred[i] * scale_time)
    total_error += np.absolute(result[i] - result_pred[i] * scale_time)

print('total error = {}'.format(total_error))