/*
Author: Vaibhav Gogte <vgogte@umich.edu>
Aasheesh Kolli <akolli@umich.edu>

ArgoDSM/PThreads version:
Ioannis Anevlavis <ioannis.anevlavis@etascale.com>
*/

#include "cq.h"

#include <cstdlib>
#include <unistd.h>
#include <sys/time.h>

#include <string>
#include <fstream>
#include <iostream>
#if DEBUG
	#include <sstream>
#endif

int workrank;
int numtasks;

// Macro for only node0 to do stuff
#define WEXEC(inst) ({ if (workrank == 0) inst; })

concurrent_queue* CQ;

void initialize() {
	CQ = new concurrent_queue;
	WEXEC(CQ->init());

	argo::barrier();

	WEXEC(fprintf(stderr, "Created cq at %p\n", (void *)CQ));
}

void* run_stub(void* ptr) {
	int ret;
	for (int i = 0; i < NUM_OPS/(NUM_THREADS*numtasks); ++i) {
		CQ->push(9+workrank);
	}
	return NULL;
}

int main(int argc, char** argv) {
	argo::init(128*1024*1024UL,128*1024*1024UL);//prev 256

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
	WEXEC(fexec << "CQ" << ", " << std::to_string((tv_end.tv_usec - tv_start.tv_usec) + (tv_end.tv_sec - tv_start.tv_sec) * 1000000) << std::endl);
	WEXEC(fexec.close());
	#if DEBUG
		WEXEC(CQ->check());
	#endif
	argo::barrier();
	//std::stringstream ss;
	//ss << workrank <<"out.txt";
	//CQ->printCQtofile((char*)ss.str().c_str());
	struct timeval tv_start2;
	struct timeval tv_end2;
	gettimeofday(&tv_start2, NULL);
	delete CQ;
	argo::barrier();
	gettimeofday(&tv_end2, NULL);
	WEXEC(fprintf(stderr, "Deconstructor time elapsed %ld us\n",
				tv_end2.tv_usec - tv_start2.tv_usec +
				(tv_end2.tv_sec - tv_start2.tv_sec) * 1000000));
	argo::finalize();

	return 0;
}
