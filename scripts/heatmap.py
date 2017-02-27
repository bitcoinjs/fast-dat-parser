import numpy as np
import matplotlib.pyplot as plt

xs = np.fromfile('utcs.dat', dtype=np.uint32)
ys = np.fromfile('values.dat', dtype=np.uint64)

print('Loaded')

xs = np.random.choice(xs, size=1000000)
ys = np.random.choice(ys, size=1000000) / 1e8
xmin = xs.min()
ymin = ys.min()
xmax = xs.max()
ymax = ys.max()

print('Sampled')

plt.scatter(xs, ys, cmap=plt.cm.YlOrRd_r)
plt.axis([xmin, xmax, ymin, ymax])
plt.title('Bitcoin Balances Over Time')

print('Ready')

plt.show()
