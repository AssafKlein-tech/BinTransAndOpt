Assaf Klein
313289555

Yuval Rosman
208253831

to compile please run:
"make obj-intel64/ex3.so PIN_ROOT=<pindir>"

to run the tool use command:
"time <pindir>/pin -t obj-intel64/ex3.so -prof -- ./bzip2 -k -f input.txt" for profiling
"time <pindir>/pin -t obj-intel64/ex3.so -inst -- ./bzip2 -k -f input.txt" for instruction translation