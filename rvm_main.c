/* multi.c - test that basic persistency works for multiple segments */
#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define SEGNAME0  "testseg1"
#define SEGNAME1  "testseg2"

#define OFFSET0  200
#define OFFSET1  600

#define STRING0 "test3"
#define STRING1 "test4"


void proc1() 
{
     rvm_t rvm;
     char* segs[2];
     trans_t trans;
     
     printf(">>> proc1()\n");

     rvm = rvm_init("rvm_segments");

     rvm_destroy(rvm, SEGNAME0);
     rvm_destroy(rvm, SEGNAME1);

     segs[0] = (char*) rvm_map(rvm, SEGNAME0, 1000);
     segs[1] = (char*) rvm_map(rvm, SEGNAME1, 1000);

     segs[0] = (char*) rvm_map(rvm, SEGNAME0, 300);

     trans = rvm_begin_trans(rvm, 2, (void **)segs);

     rvm_about_to_modify(trans, segs[0], OFFSET0, 100);
     strcpy(segs[0]+OFFSET0, STRING0);
     rvm_about_to_modify(trans, segs[1], OFFSET1, 100);
     strcpy(segs[1]+OFFSET1, STRING1);

     rvm_commit_trans(trans);

     abort();
}


void proc2() 
{
     rvm_t rvm;
     char *segs[2];

     printf(">>> proc2()\n");

     rvm = rvm_init("rvm_segments");
     segs[0] = (char*) rvm_map(rvm, SEGNAME0, 1000);
     segs[1] = (char*) rvm_map(rvm, SEGNAME1, 1000);

     if(strcmp(segs[0] + OFFSET0, STRING0)) {
	  printf("ERROR in segment 0 (%s)\n", segs[0]+OFFSET0);
	  exit(2);
     }
     if(strcmp(segs[1] + OFFSET1, STRING1)) {
	  printf("ERROR in segment 1 (%s)\n", segs[1]+OFFSET1);
	  exit(2);
     }

     printf("OK\n");
}


int main(int argc, char **argv) 
{
#if 1
     int pid;

     pid = fork();
     if(pid < 0) {
	  perror("fork");
	  exit(2);
     }
     if(pid == 0) {
	  proc1();
	  exit(0);
     }

     waitpid(pid, NULL, 0);

     proc2();
#else 
     /* truncate test */

     proc1();

     rvm_t rvm;

     printf("Before Truncation:\n");
     system("ls -l rvm_segments");
     
     rvm = rvm_init("rvm_segments");
     rvm_truncate_log(rvm);

     printf("\nAfter Truncation:\n");
     system("ls -l rvm_segments");

#endif
     return 0;

}
