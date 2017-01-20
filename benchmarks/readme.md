# Benchmarks
##Call latency to non-mobile kernel objects
This benchmark measures the latency of calls to a kernel object from the call in user space until the result of the call is available in the user space. This latency is measured for the communication of one Place with any available Place, including itself.
The ExampleHome kernel object is used for this purpose, that provides a user-callable dummy method, that immediately returns. The latency of a call to this method is measured a 10000 times. Afterwards the kernel object is relocated to the next place and the process repeats.
All measured delays are written to a file sequentially. I.e. the first 10000 values represent calls to an object located on Place 0 (HWT 0), the second 10000 values are calls to Place 1 and so on.
The result data is available in the file latency-kernel-object-non-mobile.csv

The data can be reproduced by the following commands:
```
make all micrun | sed "s,\x1B\[[0-9;]*[a-zA-Z],,g" | tee out.log
cat out.log| tr -d '\000' | grep ": user" | cut -d " " -f 3 > values.csv
```
The first command is used to remove coloring from the output to make postprocessing easier. The second command selects the correct values from the output.
