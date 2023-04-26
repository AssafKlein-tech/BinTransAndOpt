Assaf Klein
313289555

to compile please run:
"make obj-intel64/ex1.so PIN_ROOT=<pindir>"

to run the tool use command:
"time <pindir>/pin -t obj-intel64/ex1.so -- ./bzip2 -k -f input.txt"