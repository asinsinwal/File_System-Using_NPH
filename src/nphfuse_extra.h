// Project 3: Sayali Godbole, ssgodbol; Animesh Sinsinwal, assinsin; Sameer Poudwal, spoudwa; 

/*
  Copyright (C) 2016 Hung-Wei Tseng, Ph.D. <hungwei_tseng@ncsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  You may extend this file if necessary  
*/
#include <sys/stat.h>
#include <stdint.h>

#define DIR_MAX 236
#define FILE_MAX 128
#define BLOCK_SIZE   8192

struct nphfuse_state {
  FILE *logfile;
  char *device_name;
  int devfd;
};


typedef struct {
  char filename[DIR_MAX];
  char dirname[FILE_MAX];
  uint64_t offset;
  struct stat mystat;
}npheap_store;

#define TOTAL_BLOCKS  (BLOCK_SIZE/sizeof(npheap_store))
