
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include "hmalloc.h"

/*
  typedef struct hm_stats {
  long pages_mapped;
  long pages_unmapped;
  long chunks_allocated;
  long chunks_freed;
  long free_length;
  } hm_stats;
*/

typedef struct fList {
	size_t size;
	struct fList *next;
} fList;

typedef struct header {
	size_t size;
} header;

//INIT code provided by Nat's powerpoint slides
//fList* freeList = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
//freeList->size = 4096 - sizeof(fList);
//freeList->next = NULL;

const size_t PAGE_SIZE = 4096;
static hm_stats stats; // This initializes the stats to 0.

fList* freeList = NULL;

long
free_list_length()
{
    // TODO: Calculate the length of the free list.
    long len = 0;
    fList* cur = freeList;
    while(1) {
	if (cur == NULL) {
	    break;
	}
	len++;
	cur = cur->next;
    }
    return len;
}
void
coalesceFree()
{
    fList* cur = freeList;
    while(1) {
	if (cur == NULL || cur->next == NULL) {
	    return;
        }
	if (cur->next == (void*)cur + sizeof(fList) + cur->size) {
	    cur->size += sizeof(fList)+ cur->next->size;
	    cur->next = cur->next->next;
	    continue;
	   
        }
	cur = cur->next;
    }
}



void
insert_intofList(fList* newNode) 
{
    if (freeList == NULL) {
	freeList = newNode;
	return;
    }
    fList* cur = freeList;
    while(1) {
        
	if (newNode < cur) {
	    newNode->next = cur;
	    freeList = newNode;
	    break;
	} 
	else if (cur->next == NULL) {
	    cur->next = newNode;
	    break;
	}
	else if (newNode > cur && newNode < cur->next) {
	    newNode->next = cur->next;
	    cur->next = newNode;
	    break;
	}
        cur  = cur->next;
    }

    coalesceFree();
}

void*
makeOnePage(int size)
{
    stats.pages_mapped += 1;
    header* newPg = mmap(0,PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE,-1,0);
    if (PAGE_SIZE - size > sizeof(fList)) {
         
        newPg->size = size - sizeof(header);
        void* newPgFin = (void*)newPg + sizeof(header);
        fList* newFree = (void*)newPg + size;
        newFree->size = PAGE_SIZE - size - sizeof(fList);
	newFree->next = NULL;
        insert_intofList(newFree);
        return newPgFin;
    }
    else{
        newPg->size = PAGE_SIZE - sizeof(header);
        void* newPgFin = (void*)newPg + sizeof(header);
        return newPgFin;
    }
}


void*
makeMultPages(int pages, int size)
{
    stats.pages_mapped += pages;
    header* newPg = mmap(0, pages * PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE,-1,0);   
    newPg->size = pages * PAGE_SIZE - sizeof(header);
    header* newPgFin = (void*)newPg + sizeof(header);
    return newPgFin;


}

hm_stats*
hgetstats()
{
    stats.free_length = free_list_length();
    return &stats;
}

void
hprintstats()
{
    stats.free_length = free_list_length();
    fprintf(stderr, "\n== husky malloc stats ==\n");
    fprintf(stderr, "Mapped:   %ld\n", stats.pages_mapped);
    fprintf(stderr, "Unmapped: %ld\n", stats.pages_unmapped);
    fprintf(stderr, "Allocs:   %ld\n", stats.chunks_allocated);
    fprintf(stderr, "Frees:    %ld\n", stats.chunks_freed);
    fprintf(stderr, "Freelen:  %ld\n", stats.free_length);
}

static
size_t
div_up(size_t xx, size_t yy)
{
    // This is useful to calculate # of pages
    // for large allocations.
    int zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}

void*
hmalloc(size_t size)
{
    stats.chunks_allocated += 1;
    size += sizeof(header);

    if(size < sizeof(fList)) {
	size += sizeof(fList);
    }
     
    if (size < PAGE_SIZE) {
	fList* cur = freeList;
	while (1) {
	    if (cur == NULL) {
		return makeOnePage(size);
	    }
	    else {
		if (cur->size + sizeof(fList) >= size) {

		    if (freeList->size + sizeof(fList) - size >= sizeof(fList)){
		        int newSize = cur->size - size;
			freeList = (void*)freeList + size;
			freeList->size = newSize;
			header* newPg = (void*)cur;
			newPg->size = size - sizeof(header);
			void* newPgFin = (void*)newPg + sizeof(header);
			return newPgFin;
		    }
		    else {
			freeList = cur->next;
		        header* newPg = (void*)cur;
		        newPg->size = cur->size + sizeof(fList*);
		        void* newPgFin = (void*)newPg + sizeof(header);
		        return newPgFin;
		    }
		}
		else if (cur->next == NULL) {
		    return makeOnePage(size);
		}

		else if (cur->next->size + sizeof(fList) >= size) {

		    if (cur->next->size + sizeof(fList) - size >= sizeof(fList)) {
			int newSize = cur->next->size - size;
			cur->next = (void*)cur->next + size;
			cur->next->size = newSize;
			header* newPg = (header*)cur->next;
		        newPg->size = size - sizeof(header);
		        void* newPgFin = (void*)newPg + sizeof(header);
		        return newPgFin;
		    }
		    else {
		       
   		        header* newPg = (header*)cur->next;
		        newPg->size = cur->next->size + sizeof(fList*);
		        void* newPgFin = (void*)newPg + sizeof(header);
			cur->next = cur->next->next;
		        return newPgFin;
		    }
		} 
    		else {
		   cur = cur->next;
    		}
	    }
	}
    }
    else {
	int pages = div_up(size, PAGE_SIZE);
	return makeMultPages(pages, size);
    }		
}
void
hfree(void* item)
{
   
    stats.chunks_freed += 1;
    header* nn = (void*)item - sizeof(header);
    int size = nn->size;
    if (size < PAGE_SIZE) {
	fList* node = (void*)nn;
	node->size = size - sizeof(fList*);
        node->next = NULL;
        insert_intofList(node);
    }
    else {
	int pages = div_up(size, PAGE_SIZE);
	stats.pages_unmapped += pages;
	munmap(nn, size);
    }
    coalesceFree();
}









