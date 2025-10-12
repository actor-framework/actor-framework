# cuda-actors
Cuda Actors is a way of bringing the Actor Model of scientific Computation to the GPU.
Cuda Actors aims to extend CAF's resource utilization,fault tolerance and high performance nature to the GPU.
The current version of CAF that is supported is CAF 1.1.0 and all the code related to cuda actors 
can be found in actor-framework/libcaf_cuda.


## Features
- create your own custom GPU actor or use the built in GPU actor facade
- multi GPU and multi stream support 
- auto GPU memory management as well as send other actors memory on the GPU for sophisicated GPU pipelines 
- shared memory support

## Getting started
If you are new to cuda actors you can check out the documentaion documentation.txt or the example files in the examples directory.
Otherwise use the cmake files to build and run cuda actors 
