Maroon Ayoub
315085597

Carlos Kassis
207260761

How to compile:
- Place .cpp, makefile, makefile.rules inside <PINDIR>/source/tools/<WORKDIR> folder.
- From <PINDIR>/source/tools/<WORKDIR> folder, Run:
$ make ex2.test

How to run:
- From inside <PINDIR>/source/tools/<WORKDIR>/obj-intel64 folder, Run:
$ ../../../../pin -t ex2.so -- ./bzip2 -k -f input.txt
