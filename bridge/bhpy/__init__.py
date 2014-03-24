from array_create import *
from ufunc import ufuncs
from ndarray import check
from _info import numpy_types

#Expose all ufuncs
for f in ufuncs:
    exec "%s = f"%f.info['np_name']

#Expose all data types
for t in numpy_types:
    exec "%s = t"%t.__str__()
