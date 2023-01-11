/**
 * virtmem.c
 */

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define TLB_SIZE 16
#define PAGES 1024
#define PAGE_MASK 0xFFC00 /* TODO  = 11111111110000000000 */
#define VIRTUAL_ADRESS_FRAME_SIZE 1024
#define PAGE_SIZE 256
#define OFFSET_BITS 10
#define OFFSET_MASK 0x3FF /* TODO  =1111111111 */

#define MEMORY_SIZE PAGES * PAGE_SIZE

// Max number of characters per line of input file to read.
#define BUFFER_SIZE 10



struct tlbentry {
  int logical;
  int physical;
};

// TLB is kept track of as a circular array, with the oldest element being overwritten once the TLB is full.
struct tlbentry tlb[TLB_SIZE];
// number of inserts into TLB that have been completed. Use as tlbindex % TLB_SIZE for the index of the next TLB line to use.
int tlbindex = 0;
int page_index_counter=0;

// pagetable[logical_page] is the physical page number for logical page. Value is -1 if that logical page isn't yet in the table.
int pagetable[PAGES];

int usages[PAGES/(VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE)];
//int usages[PAGES];
char * pageReplacementAlgorithm;
int total_addresses=0;
char tempChar;

signed char main_memory[MEMORY_SIZE];

// Pointer to memory mapped backing file
signed char *backing;

int max(int a, int b)
{
  if (a > b)
    return a;
  return b;
}

/* Returns the physical address from TLB or -1 if not present. */
int search_tlb(int logical_page) {
    /* TODO */
    
    for( int i = 0; i<TLB_SIZE; i++ ) {
        if(tlb[i].logical == logical_page)
            return tlb[i].physical;
    }
    
    return -1;
}

/* Adds the specified mapping to the TLB, replacing the oldest mapping (FIFO replacement). */
void add_to_tlb(int logical, int physical) {
    /* TODO */
    tlb[tlbindex % TLB_SIZE].logical = logical;
    tlb[tlbindex % TLB_SIZE].physical = physical;
    tlbindex++;
}

int findPhysicalPage(int logical_page) {
    /* TODO */
    
    for( int i = 0; i<PAGES; i++ ) {
        if(pagetable[i] == logical_page){
            return i;
        }
    }
    
    return -1;
}

int findLeastRecentlyUsed(){
    
    int smallest = 0;
    int index = 0;
    
    //printf("Usage length : % d",sizeof(usages));
 
    smallest = usages[0];
    
    //printf(" [USAGE(%d): %d]",0,usages[0]);
    
    for(int i=1; i< PAGES/(VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE);i++)
    {
       // printf(" [USAGE(%d): %d]",i,usages[i]);
        if (usages[i] < smallest){
            smallest = usages[i];
            index = i;
        
        }
    }
   // printf("\nINDEX: % d , USAGE SIZE : %d\n",index,PAGES/(VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE));
    return index;
    
}

int getReplacingPhysicalPageUsingFIFO(){
    int rt = page_index_counter++;
    if (rt < (PAGES/(VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE))){
        
        return (VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE)*(rt);
    }else{
        return (VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE)*((rt % (PAGES/(VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE))));
    }
}

int getReplacingPhysicalPageUsingLRU(){
    if (page_index_counter < (PAGES/(VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE))){
    //if (total_addresses <= (PAGES)){
        usages[page_index_counter] = total_addresses;
        ///////////////////////////////////////////////////////////////////////////////printf("TOTAL ADRESSES %d---------------------------------------------------\n",total_addresses);
        int rt = page_index_counter++;
       // printf("RT : %d - INDEX : %d---------------------------------------------------\n",rt,page_index_counter);
        return (VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE)*(rt);
       
        //return total_addresses-1;
        
    }else{
        int pa = findLeastRecentlyUsed();
        usages[pa] = total_addresses;
        ///////////////////////////////////////////////////////////////////////////////////////printf("TOTAL ADRESSES %d , PA: %d---------------------------------------------------\n",total_addresses,pa);
        return (VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE)*(pa);
    }}

int determinePhysicalPage(){
    
    if (pageReplacementAlgorithm[0] == '0'){  // FIFO
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////printf("FIFO ALGORITHM---------------------------------------------------\n");
        return getReplacingPhysicalPageUsingFIFO();
    }else                                // LRU
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////printf("LRU ALGORITHM---------------------------------------------------\n");
        return getReplacingPhysicalPageUsingLRU();
    }
}

int main(int argc, const char *argv[])
{
  if (argc != 5) {
    fprintf(stderr, "Usage ./virtmem backingstore input\n");
    exit(1);
  }
    
    pageReplacementAlgorithm = argv[4];
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////printf("pageReplacementAlgorithm : %d (%c)\n",pageReplacementAlgorithm[0],pageReplacementAlgorithm[0]);
   

    const char *backing_filename = argv[1];
  //int backing_fd = open(backing_filename, O_RDONLY);
    FILE * backing_fd = fopen(backing_filename, "rb");
    //backing = mmap(0, MEMORY_SIZE, PROT_READ, MAP_PRIVATE, backing_fd, 0);
    /*
    for (int i = 0; i < 100; i++) {
            char c = '-';

        fread(&c, sizeof(char), 1, backing_fd);
        printf("Readed char ( %d ) : %d - (%c)",i,c,c);
        }
  */
  const char *input_filename = argv[2];
  FILE *input_fp = fopen(input_filename, "r");
  
  // Fill page table entries with -1 for initially empty table.
  int i;
  for (i = 0; i < PAGES; i++) {
    pagetable[i] = -1;

 }
    
  
  for (i = 0; i<(PAGES/(VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE)); i++) {
     usages[i] = 0;
  }
   
    // Character buffer for reading lines of input file.
  char buffer[BUFFER_SIZE];
  
  // Data we need to keep track of to compute stats at end.
  total_addresses = 0;
  int tlb_hits = 0;
  int page_faults = 0;
  
  // Number of the next unallocated physical page in main memory
  int free_page = 0;
    /*
    while (fgets(buffer, BUFFER_SIZE, input_fp) != NULL) {
        total_addresses++;
        int logical_address = atoi(buffer);
        
        
         // Calculate the page offset and logical page number from logical_address
        
        int offset = (logical_address & OFFSET_MASK);              // and ile
        int logical_page =    (((logical_address) & (PAGE_MASK)) >> OFFSET_BITS);       // shift ile
        printf("Virtual address: %d Logical Page: %d OFFSET: %d\n", logical_address, logical_page, offset);
    }
    exit(0);
    */
    while (fgets(buffer, BUFFER_SIZE, input_fp) != NULL) {
    total_addresses++;
    int logical_address = atoi(buffer);

    /* TODO
    / Calculate the page offset and logical page number from logical_address */
       
      int offset = (logical_address & OFFSET_MASK);              // and ile
      int logical_page =    (((logical_address) & (PAGE_MASK)) >> OFFSET_BITS);       // shift ile
     //printf("Virtual address: %d Logical Page: %d OFFSET: %d\n", logical_address, logical_page, offset);
  
    
    int physical_page = search_tlb(logical_page);
      
    // TLB hit
    if (physical_page != -1) {
      tlb_hits++;
       // printf("TLB HIT : %d (%d)\n",logical_address,physical_page);
        usages[physical_page/(VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE)] = total_addresses;
        // TLB miss
    } else {
      //physical_page = logicalTophysicalMatchMap[logical_page];
      // printf("TLB MISS : %d (%d)\n",logical_address,physical_page);
        //physical_page = logicalTophysicalMatchMap[logical_address];
        physical_page = findPhysicalPage(logical_page);
        // Page fault
      if (physical_page == -1) {
          /* TODO */
         //printf("PAGE FAULT : %d (%d)\n",logical_address,physical_page);
          page_faults++;
          
         
          
          //fseek(backing_fd, (logical_page*VIRTUAL_ADRESS_FRAME_SIZE)+ (PAGE_SIZE* (offset/PAGE_SIZE)), SEEK_SET);
          fseek(backing_fd, (logical_page*VIRTUAL_ADRESS_FRAME_SIZE), SEEK_SET);
          
          ////////////////////////////////////////////////////////////////////////////////printf("Determining physical page.\n");
          physical_page = determinePhysicalPage();
          ////////////////////////////////////////////////////////////////////////////////printf("Physical page : (%d).\n",physical_page);
          pagetable[physical_page] = logical_page;
          pagetable[physical_page+1] = logical_page;
          pagetable[physical_page+2] = logical_page;
          pagetable[physical_page+3] = logical_page;
          
          
          //logicalTophysicalMatchMap[logical_address] = physical_page;
         
          //physical_page_counter = physical_page-1;
          int k=0;
          for (int i=0; i<(VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE);i++){
              
              for(int j=0; j<PAGE_SIZE;j++)
              {
                  tempChar = '-';
                  fread(&tempChar, sizeof(char), 1, backing_fd);
                  if (tempChar != EOF){
                      main_memory[physical_page*PAGE_SIZE+(k++)]=tempChar;//???????????????
                      //main_memory[physical_page*PAGE_SIZE+j]=tempChar;//???????????????
                      
                  }else
                  {
                      break;
                  }
              }
              if (tempChar == EOF){
                  break;
             }
          }
         
          
      }else
      {
         //printf("PAGE HIT : %d (%d)\n",logical_address,physical_page);
         usages[physical_page/(VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE)] = total_addresses;
      }
          
      
      add_to_tlb(logical_page, physical_page);
    }
        
        int physical_address = (physical_page/(VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE)<< OFFSET_BITS) | offset;
    //signed char value = main_memory[(physical_page * PAGE_SIZE) + (offset % PAGE_SIZE)];
        signed char value = main_memory[(physical_page/(VIRTUAL_ADRESS_FRAME_SIZE/PAGE_SIZE))*1024 + (offset)];
    
    //printf("Virtual address ():  Offset :  Physical address:  Value:  ()\n");
    //printf(" VA(%d): %d Off : %d Physical: %d Value: %d (%c)\n",total_addresses,logical_page,offset, physical_page, value,value);
      //printf("Virtual address (%d): %d Physical address: %d Value: %d\n", total_addresses,logical_address, physical_address, value);
        printf("Virtual address: %d Physical address: %d Value: %d\n",logical_address, physical_address, value);
        
    }
  
    for( int i = 0; i<PAGES; i++ ) {
        ////////////////////////////////////////////////////////////////printf("page(%d) : (%d) , adress : (%d)\n",i,pagetable[i]);
        
    }
  printf("Number of Translated Addresses = %d\n", total_addresses);
  printf("Page Faults = %d\n", page_faults);
  printf("Page Fault Rate = %.3f\n", page_faults / (1. * total_addresses));
  printf("TLB Hits = %d\n", tlb_hits);
  printf("TLB Hit Rate = %.3f\n", tlb_hits / (1. * total_addresses));
  //printf("filename : %s\n",backing_filename);
  return 0;
}

