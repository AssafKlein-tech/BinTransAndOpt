#include <stdio.h>

#define MAX_CALLS 1000

void foo()
{
  int count = 0;
  for ( int j = 0; j < 10;j++)
  {
    count ++;
  }

}


void bar()
{
  foo();

}


void gal()
{

 bar();

}

void main()
{

 int i,j;

 for (j=0; j < 4; j++)
   for (i=0; i < MAX_CALLS; i++)
     gal();

}
