#include <sys/mman.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#define PAGES        256
#define BUFFER_SIZE  10
#define PAGE_SIZE    256 //2^8
#define OFFSET_BITS  8
#define OFFSET_MASK  PAGE_SIZE - 1
#define SIZE 4
#define MEMO_SIZE 65536
#define TLB_SIZE 16


char * mmapfptr;
signed char main_Memo[MEMO_SIZE];

//define a new TLBentry with pair(pagetable, frametable)
typedef struct TLBentry{
    int pagetable;
    int frametable;
} TLBentry;

//search_TLB function
//return the related pagenumber. if dne return -1
int search_TLB(int pagenumber, TLBentry tlb[16]){
    for(int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].pagetable == pagenumber) {
            return tlb[i].frametable;
        }
    }
    return -1;
}

//TLB_add function
//take a tlbpointer as argument which can track the oldest TLBentry
void TLB_add(int pagenumber, int framenumber, int pointer, TLBentry tlb[16]){
    tlb[pointer].pagetable = pagenumber;
    tlb[pointer].frametable = framenumber;
}

//TLB_update function
//update the pagenumber in where it is
int TLB_update(int pagenumber, int framenumber, TLBentry tlb[16]) {
    for(int i = 0; i < TLB_SIZE; i++) {
        if(tlb[i].frametable == framenumber) {
            tlb[i].pagetable = pagenumber;
            return 0;
        }
    }
    return -1;
}

int main(int argc, char const *argv[]) {
    
    char buffer[BUFFER_SIZE];

    unsigned int page_number, frame_number, virtual_address,
                 physical_address;

    int offset;

    int page_table[PAGES];


    //BACKING_STORE operations
    int mmapfile_fd = open("BACKING_STORE.bin", O_RDONLY);
    
    mmapfptr = mmap(0,MEMO_SIZE, PROT_READ, MAP_PRIVATE, mmapfile_fd, 0);
    
    //initial page table with -1
    for(int i = 0; i < PAGE_SIZE; i++){
        page_table[i] = -1;
    }

    FILE *file_pointer = fopen("addresses.txt", "r");

    //initial tlb with -1
    TLBentry tlb[TLB_SIZE];
    for(int i = 0; i < TLB_SIZE; i++) {
        tlb[i].frametable = -1;
        tlb[i].pagetable = -1;
    }

    int tlbHit = 0;
    int total_addr = 0;
    int pageFault = 0;
    unsigned char pagePointer = 0;
    unsigned char tlbPointer = 0;
    while (fgets(buffer, BUFFER_SIZE, file_pointer) != NULL) {
        
        virtual_address = atoi(buffer);

        offset = OFFSET_MASK & virtual_address;

        page_number = (virtual_address >> OFFSET_BITS);

        //firt lookup in tlbarray. If dne, we can look up in pagetable
        int serachTLBresult = search_TLB(page_number, tlb);
        if (serachTLBresult != -1) {
            frame_number = serachTLBresult; //framenumber
            tlbHit++;
        } else {
            frame_number = page_table[page_number];
        
            if(frame_number == -1) {
                pageFault++;
                //the page talbe is a circular array with a pagePointer, when the pagePointer == Size, pagePointer bakcs to 0
                if (pagePointer == 128) {
                    pagePointer = 0;
                }

                frame_number = pagePointer;
                //delte the existed framenumber
                for (int i = 0; i < PAGES; i++) {
                    if ( page_table[i] == frame_number) {
                        page_table[i] = -1;
                    }
                }
                //everytime we add an new entry to the pagetable, tlb += 1
                pagePointer++;
            }

            //tlb is also a cicular array with tlbPointer
            //if we can find the framenumber in tlb, we can updatede the table in place
            //if dne, we can add a new entry to the tlb array
            if (tlbPointer == TLB_SIZE) {
                tlbPointer = 0;
                int flag = TLB_update(page_number, frame_number, tlb);
                if (flag == -1) {
                    TLB_add(page_number, frame_number, tlbPointer, tlb);
                    tlbPointer++;
                }
            } else {
                TLB_add(page_number, frame_number, tlbPointer, tlb);
                //everytime we add an new entry to the tlb using tlb_add function, tlb += 1
                tlbPointer++;
            }

        }

        memcpy(main_Memo + frame_number * PAGE_SIZE, mmapfptr + page_number * PAGE_SIZE, PAGE_SIZE);
        page_table[page_number] = frame_number;

        int value = main_Memo[frame_number * PAGE_SIZE + offset];

        physical_address = (frame_number << OFFSET_BITS) | offset;

        printf("Virtual address: %d, Physical address = %d Value=%d\n", virtual_address, physical_address, value);
        
        total_addr ++;

    }
    printf("Total address = %d\n", total_addr);
    printf("Page_faults = %d\n", pageFault);
    printf("TLB Hits = %d\n", tlbHit);

    fclose(file_pointer);
    return 0;
}