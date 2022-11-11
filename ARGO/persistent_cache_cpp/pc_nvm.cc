/*
Author: Vaibhav Gogte <vgogte@umich.edu>
Aasheesh Kolli <akolli@umich.edu>

ArgoDSM/PThreads version:
Ioannis Anevlavis <ioannis.anevlavis@etascale.com>

This file uses the PersistentCache to enable multi-threaded updates to it.
*/

#include "argo.hpp"
#include "cohort_lock.hpp"

#include <cstdint>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

#include <string>
#include <fstream>
#include <iostream>

#define NUM_ELEMS_PER_DATUM 2
//#define NUM_ROWS 1048608// 6 nodes
//#define NUM_ROWS 262144 // 1 node
//#define NUM_ROWS 524288 //2 Node
#define NUM_ROWS 1048576 //4 nodes



//#define NUM_UPDATES 524352 // 6 nodes
//#define NUM_UPDATES 131072 //1 node
//#define NUM_UPDATES 262144 //2 nodes
#define NUM_UPDATES 524288 //4 Nodes
#define NUM_THREADS 4

#define MOD_ARGO 1 //Modified ARGO version for mass allocation of locks flags.
#define EFFICIENT_INITIALIZATION 1 //More efficient allocation of the elements by using conew_array over invidividual allocations.
#define SELECTIVE_ACQREL 1
#define DEBUG 0

int workrank;
int numtasks;

// Macro for only node0 to do stuff
#define WEXEC(inst) ({ if (workrank == 0) inst; })

void distribute(int& beg,
		int& end,
		const int& loop_size,
		const int& beg_offset,
    		const int& less_equal){
	int chunk = loop_size / numtasks;
	beg = workrank * chunk + ((workrank == 0) ? beg_offset : less_equal);
	end = (workrank != numtasks - 1) ? workrank * chunk + chunk : loop_size;
}

// Persistent cache organization. Each Key has an associated Value in the data array.
// Each datum in the data array consists of up to 8 elements.
// The number of elements actually used by the application is a runtime argument.
struct Element {
	int32_t value_[NUM_ELEMS_PER_DATUM];
};

struct Datum {
	// pointer to the hashmap
	Element* elements_;
	// A lock which protects this Datum
	argo::globallock::cohort_lock* lock_;
};

struct pc {
	Datum* hashmap;
	int num_rows_;
	int num_elems_per_row_;
};

uint32_t pc_rgn_id;
pc* P;
#if MOD_ARGO
	argo::globallock::global_tas_lock::internal_field_type* lockptrs; //lockptr for more efficient initialization, only sued when MOD_ARGO is set to 1.
#endif
#if EFFICIENT_INITIALIZATION
	Element* RowElements;
#endif
//Verification function
void check() {
		int checksum = 0;
		for (int i = 0; i<NUM_ROWS; i++) {
			for(int y = 0; y<NUM_ELEMS_PER_DATUM; y++) {
				checksum += P->hashmap[i].elements_->value_[y];
			}
		}
		int expectedsum = 0;
		int chunk = (NUM_UPDATES/(NUM_THREADS*numtasks))-1; //125
		expectedsum = (NUM_ELEMS_PER_DATUM-1 * (NUM_ELEMS_PER_DATUM-1 + 1)/2)*NUM_UPDATES + 2*(NUM_THREADS*numtasks)*(chunk * (chunk + 1)/2);
		assert(checksum == expectedsum);
		std::cout<<"Verification succesful!"<<std::endl;
}
//End of verification function

void datum_init(pc* p) {
	#if MOD_ARGO
		//argo::globallock::global_tas_lock is equivelant to global_lock_type at line 47 in synchronization/cohort_lock.hpp
		lockptrs = argo::conew_array<argo::globallock::global_tas_lock::internal_field_type>(NUM_ROWS);
	#endif
	#if EFFICIENT_INITIALIZATION
		RowElements = argo::conew_array<Element>(NUM_ROWS);
	#endif
	for(int i = 0; i < NUM_ROWS; i++) {
		#if EFFICIENT_INITIALIZATION
			p->hashmap[i].elements_ = &RowElements[i]; //More efficient allocation of the elements by using conew_array over invidividual allocations.
		#else
			p->hashmap[i].elements_ = argo::conew_<Element>();
		#endif

		#if SELECTIVE_ACQREL
			#if !MOD_ARGO
				p->hashmap[i].lock_ = new argo::globallock::cohort_lock(true);
			#else
				p->hashmap[i].lock_ = new argo::globallock::cohort_lock(&lockptrs[i],true);
			#endif
		#else
			#if !MOD_ARGO
				p->hashmap[i].lock_ = new argo::globallock::cohort_lock();
			#else
				p->hashmap[i].lock_ = new argo::globallock::cohort_lock(&lockptrs[i]);
			#endif
		#endif
	}
	#if EFFICIENT_INITIALIZATION
		argo::barrier();
	#endif
	WEXEC(std::cout << "Finished allocating elems & locks" << std::endl);

	int beg, end;
	distribute(beg, end, NUM_ROWS, 0, 0);

	std::cout << "rank: "                << workrank
	          << ", initializing from: " << beg
		  << " to: "                 << end
		  << std::endl;

	for(int i = beg; i < end; i++)
		for(int j = 0; j < NUM_ELEMS_PER_DATUM; j++)
			p->hashmap[i].elements_->value_[j] = 0;
	WEXEC(std::cout << "Finished team process initialization" << std::endl);
}

void datum_free(pc* p) {
	for(int i = 0; i < NUM_ROWS; i++) {
		delete p->hashmap[i].lock_;
		argo::codelete_(p->hashmap[i].elements_);
	}
	#if MOD_ARGO
		argo::codelete_array(lockptrs);
	#endif
	#if EFFICIENT_INITIALIZATION
		argo::codelete_array(RowElements);
	#endif
}

void datum_set(int key, int value) {
	P->hashmap[key].lock_->lock();
	#if SELECTIVE_ACQREL
		argo::backend::selective_acquire(P->hashmap[key].elements_, sizeof(Element));
	#endif

	for(int j = 0; j < NUM_ELEMS_PER_DATUM; j++)
	#if DEBUG
		P->hashmap[key].elements_->value_[j] += value+j;
	#else
		P->hashmap[key].elements_->value_[j] = value+j;
	#endif
	#if SELECTIVE_ACQREL
		argo::backend::selective_release(P->hashmap[key].elements_, sizeof(Element));
	#endif

	P->hashmap[key].lock_->unlock();
}

void initialize() {
	P = new pc;
	P->num_rows_ = NUM_ROWS;
	P->num_elems_per_row_ = NUM_ELEMS_PER_DATUM;
	P->hashmap = new Datum[NUM_ROWS];
	datum_init(P);
	argo::barrier();

	WEXEC(fprintf(stderr, "Created hashmap at %p\n", (void *)P->hashmap));
}

void* CacheUpdates(void* arguments) {
	int key;
	for (int i = 0; i < NUM_UPDATES/(NUM_THREADS*numtasks); i++) {
		key = rand()%NUM_ROWS;
		datum_set(key, i);
	}
	return 0;
}

int main (int argc, char* argv[]) {
	argo::init(500*1024*1024UL,500*1024*1024UL);

	workrank = argo::node_id();
	numtasks = argo::number_of_nodes();
	#if SELECTIVE_ACQREL
		WEXEC(printf("Running selective coherence version \n"));
	#endif
	WEXEC(std::cout << "In main\n" << std::endl);
	struct timeval tv_start;
	struct timeval tv_end;

	std::ofstream fexec;
	WEXEC(fexec.open("exec.csv",std::ios_base::app));

	// This contains the Atlas restart code to find any reusable data
	initialize();
	WEXEC(std::cout << "Done with cache creation" << std::endl);

	pthread_t threads[NUM_THREADS];

	gettimeofday(&tv_start, NULL);
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_create(&threads[i], NULL, CacheUpdates, NULL);
	}
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}
	argo::barrier();
	gettimeofday(&tv_end, NULL);

	WEXEC(fprintf(stderr, "time elapsed %ld us\n",
				tv_end.tv_usec - tv_start.tv_usec +
				(tv_end.tv_sec - tv_start.tv_sec) * 1000000));
	WEXEC(fexec << "PC" << ", " << std::to_string((tv_end.tv_usec - tv_start.tv_usec) + (tv_end.tv_sec - tv_start.tv_sec) * 1000000) << std::endl);
	WEXEC(fexec.close());
	#if DEBUG
		WEXEC(check());
		argo::barrier();
	#endif

	datum_free(P);
	delete[] P->hashmap;
	delete P;

	WEXEC(std::cout << "Done with persistent cache" << std::endl);

	argo::finalize();

	return 0;
}
