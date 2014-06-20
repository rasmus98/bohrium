import bohrium as np
import bohrium.linalg as la
import util

B = util.Benchmark()
N = B.size[0]

a = np.random.random((N,N),dtype=B.dtype,bohrium=B.bohrium)
B.start()
(l,u) = la.lu(a)
B.stop()
B.pprint()
