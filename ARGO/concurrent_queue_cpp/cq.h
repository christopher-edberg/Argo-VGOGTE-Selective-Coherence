/*
Author: Vaibhav Gogte <vgogte@umich.edu>
Aasheesh Kolli <akolli@umich.edu>

ArgoDSM/PThreads version:
Ioannis Anevlavis <ioannis.anevlavis@etascale.com>
*/

#include "argo.hpp"
#include "cohort_lock.hpp"

#include <cstdlib>
#include <cstdint>
#include <pthread.h>

#include <vector>

#define NUM_SUB_ITEMS 64
#define NUM_OPS 131072//65536//524288//262144//131072//9984 // has to be evenly divisible by number of nodes* threads.
#define NUM_THREADS 4

#define SELECTIVE_ACQREL 1 //0 for normal full coherence, 1 for selective coherence in critical sections.
#define DEBUG 0

typedef struct item item;

struct sub_item {
	int val;
	sub_item& operator=(sub_item& other) {
		val = other.val;
		return *this;
	}
};

struct item {
	item* next;
	sub_item si[NUM_SUB_ITEMS];
	item() {
		next = NULL;
	};
	item& operator=(item& other) {
		for (int i= 0; i< NUM_SUB_ITEMS; i++) {
			*(si + i) = *(other.si + i);
		}
		return *this;
	}
};

class concurrent_queue {
	item **head;
	item **tail;
	argo::globallock::cohort_lock *enq_lock;
	argo::globallock::cohort_lock *deq_lock;
	int num_sub_items;

	public:
	concurrent_queue();
	~concurrent_queue();
	void push(int);
	bool pop(int&);
	void init();
	void check();

	void printCQ(); //Prints CQ.
	void write_to_file(std::string string, char* output = NULL); //Prints strings to files.
	void printCQtofile(char* output = NULL);
	int returnHead();
	item* returnNext();
};
