/*
Author: Vaibhav Gogte <vgogte@umich.edu>
Aasheesh Kolli <akolli@umich.edu>

ArgoDSM/PThreads version:
Ioannis Anevlavis <ioannis.anevlavis@etascale.com>

This microbenchmark swaps two items in an array.
*/

#include "argo.hpp"
#include "cohort_lock.hpp"

#include <cstdint>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#include <string>
#include <fstream>
#include <iostream>
//524352//524288//131072//10000
//1048608//1048576//1000000
#define NUM_SUB_ITEMS 64
#define NUM_OPS 131072 //1 node
//#define NUM_OPS 262144 //2 nodes
//#define NUM_OPS 524288 //4 Nodes

#define NUM_ROWS 262144 // 1 node
//#define NUM_ROWS 524288 //2 Node
//#define NUM_ROWS 1048576 //4 nodes
#define NUM_THREADS 8

#define ENABLE_VERIFICATION 0	//Enable/Disable verification functions.
#define SELECTIVE_ACQREL 0
#define MOD_ARGO 1 //Modified ARGO version for mass allocation of locks flags.
#define EFFICIENT_INITIALIZATION 1 //More efficient allocation of the elements by using conew_array over individual allocations.
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

struct Element {
	// An int variable used for verification.
	#if ENABLE_VERIFICATION == 1
		int verification = 0;
	#endif
	int32_t value_[NUM_SUB_ITEMS];
	Element& operator=(Element& other) {
		for (int i= 0; i< NUM_SUB_ITEMS; i++) {
			*(value_ + i) = *(other.value_ + i);
		}
		return *this;
	}
};

struct Datum {
	// pointer to the hashmap
	Element* elements_;
	// A lock which protects this Datum
	argo::globallock::cohort_lock* lock_;
};

struct sps {
	Datum* array;
	int num_rows_;
	int num_sub_items_;
};

sps* S;
#if MOD_ARGO
	argo::globallock::global_tas_lock::internal_field_type* lockptrs; //lockptr for more efficient initialization, only sued when MOD_ARGO is set to 1.
#endif
#if EFFICIENT_INITIALIZATION
	Element* RowElements;//More efficient allocation of the elements by using conew_array over invidividual allocations.
#endif
void datum_init(sps* s) {
	#if MOD_ARGO
		//argo::globallock::global_tas_lock is equivelant to global_lock_type at line 47 in synchronization/cohort_lock.hpp
		lockptrs = argo::conew_array<argo::globallock::global_tas_lock::internal_field_type>(NUM_ROWS);
	#endif
	#if EFFICIENT_INITIALIZATION
		RowElements = argo::conew_array<Element>(NUM_ROWS);
	#endif
	for(int i = 0; i < NUM_ROWS; i++) {
		#if EFFICIENT_INITIALIZATION
			s->array[i].elements_ = &RowElements[i]; //More efficient allocation of the elements by using conew_array over invidividual allocations.
		#else
			s->array[i].elements_ = argo::conew_<Element>();
		#endif
		#if SELECTIVE_ACQREL
			#if !MOD_ARGO
				s->array[i].lock_ = new argo::globallock::cohort_lock(true);
			#else
				s->array[i].lock_ = new argo::globallock::cohort_lock(&lockptrs[i],true);
			#endif
		#else
			#if !MOD_ARGO
				s->array[i].lock_ = new argo::globallock::cohort_lock();
			#else
				s->array[i].lock_ = new argo::globallock::cohort_lock(&lockptrs[i]);
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
		for(int j = 0; j < NUM_SUB_ITEMS; j++)
			s->array[i].elements_->value_[j] = i+j;
	WEXEC(std::cout << "Finished team process initialization" << std::endl);
}

void datum_free(sps* s) {
	for(int i = 0; i < NUM_ROWS; i++) {
		delete s->array[i].lock_;
		argo::codelete_(s->array[i].elements_);
	}
	#if MOD_ARGO
		argo::codelete_array(lockptrs);
	#endif
	#if EFFICIENT_INITIALIZATION
		argo::codelete_array(RowElements);
	#endif
}

void initialize() {
	S = new sps;
	S->num_rows_ = NUM_ROWS;
	S->num_sub_items_ = NUM_SUB_ITEMS;
	S->array = new Datum[NUM_ROWS];
	datum_init(S);
	argo::barrier();
	WEXEC(fprintf(stderr, "Created array at %p\n", (void *)S->array));
}

bool swap(unsigned int index_a, unsigned int index_b) {
	//check if index is out of array
	assert (index_a < NUM_ROWS && index_b < NUM_ROWS);

	//exit if swapping the same index
	if (index_a == index_b) {
		#if ENABLE_VERIFICATION == 1
			S->array[index_a].lock_->lock();
			#if SELECTIVE_ACQREL
				argo::backend::selective_acquire(S->array[index_a].elements_, sizeof(Element));
			#endif

			S->array[index_a].elements_->verification = S->array[index_a].elements_->verification+ 2;

			#if SELECTIVE_ACQREL
				argo::backend::selective_release(S->array[index_a].elements_, sizeof(Element));
			#endif
			S->array[index_a].lock_->unlock();
		#endif
		return true;
	}
	//enforce index_a < index_b
	if (index_a > index_b) {
		int index_tmp = index_a;
		index_a = index_b;
		index_b = index_tmp;
	}
	//std::cout<<gettid()<<"Entering lock"<<std::endl;
	S->array[index_a].lock_->lock();  //#changed Changed to selective acq/rel.
	S->array[index_b].lock_->lock();

	#if SELECTIVE_ACQREL
		argo::backend::selective_acquire(S->array[index_a].elements_, sizeof(Element));//sizeof(Datum));
		argo::backend::selective_acquire(S->array[index_b].elements_, sizeof(Element));
	#endif
	//swap array values
	Element temp;
	temp = *(S->array[index_a].elements_);
	*(S->array[index_a].elements_) = *(S->array[index_b].elements_);
	*(S->array[index_b].elements_) = temp;

	#if ENABLE_VERIFICATION == 1
		S->array[index_a].elements_->verification = S->array[index_a].elements_->verification+ 1;
		S->array[index_b].elements_->verification = S->array[index_b].elements_->verification+ 1;
	#endif

	#if SELECTIVE_ACQREL
		argo::backend::selective_release(S->array[index_a].elements_, sizeof(Element));
		argo::backend::selective_release(S->array[index_b].elements_, sizeof(Element));
	#endif

	S->array[index_a].lock_->unlock();
	S->array[index_b].lock_->unlock();
	return true;
}

void* run_stub(void* ptr) {
	for (int i = 0; i < NUM_OPS/(NUM_THREADS*numtasks); ++i) {
		int index_a = rand()%NUM_ROWS;
		int index_b = rand()%NUM_ROWS;
		swap(index_a, index_b);
	}
	return NULL;
}


//For verification purposes #Verification #changed
#if ENABLE_VERIFICATION
void verify() {
	long acc = 0;
	for (long i = 0; i < NUM_ROWS; ++i)
		acc += S->array[i].elements_->verification;

	long ops = NUM_THREADS*numtasks;
	ops *= NUM_OPS/(NUM_THREADS*numtasks);

	if (acc == 2*ops)
		std::cout << "VERIFICATION: SUCCESS" << std::endl;
	else
		std::cout << "VERIFICATION: FAILURE" << std::endl;
}
#endif
//end of verification function


int main(int argc, char** argv) {
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
	pthread_t threads[NUM_THREADS];

	gettimeofday(&tv_start, NULL);
	for (int i = 0; i < NUM_THREADS; ++i) {
		pthread_create(&threads[i], NULL, &run_stub, NULL);
	}
	for (int i = 0; i < NUM_THREADS; ++i) {
		pthread_join(threads[i], NULL);
	}
	argo::barrier();
	gettimeofday(&tv_end, NULL);

	WEXEC(fprintf(stderr, "time elapsed %ld us\n",
				tv_end.tv_usec - tv_start.tv_usec +
				(tv_end.tv_sec - tv_start.tv_sec) * 1000000));
	WEXEC(fexec << "SPS" << ", " << std::to_string((tv_end.tv_usec - tv_start.tv_usec) + (tv_end.tv_sec - tv_start.tv_sec) * 1000000) << std::endl);
	WEXEC(fexec.close());


	#if ENABLE_VERIFICATION == 1
		WEXEC(verify());
	#endif

	datum_free(S);
	delete[] S->array;
	delete S;

	argo::finalize();

	return 0;
}
