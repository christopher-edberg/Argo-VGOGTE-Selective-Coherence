/*
Author: Vaibhav Gogte <vgogte@umich.edu>
Aasheesh Kolli <akolli@umich.edu>

ArgoDSM/PThreads version:
Ioannis Anevlavis <ioannis.anevlavis@etascale.com>
*/

#include "cq.h"

#include <iostream>
//Functions for verification purposes
void concurrent_queue::printCQ() {
	item *items = (*head)->next;
	for (int i = 0; i < NUM_OPS; i++) {
		for(int y = 0; y < num_sub_items; y++){
			std::cout<<(items->si+y)->val<<" ";
		}
		std::cout<<std::endl;
		items = items->next;
	}
}
// sort -n -k2 out.txt > sorted.txt to sort list.
// cut -d' ' -f1 "sorted.txt" | sort | uniq -c | sort -rn > occurances.txt for list of occurences.
// sort -n -k2 occurances.txt > sorted.txt
//Alternatively run : sort -n -k2 out.txt | cut -d' ' -f1 | sort | uniq -c | sort -rn | sort -n -k2 | cut -c 7- > sorted.txt
void concurrent_queue::printCQtofile(char* output)
{
	int i, rv,rv2;
	FILE *file;
	char *outputFile;
	if(output == NULL) {
		outputFile = (char*)"out.txt";
	}
	else {
		outputFile =output;
	}
	file = fopen(outputFile, "w");
	if(file == NULL) {
		printf("ERROR: Unable to open file `%s'.\n", outputFile);
		exit(1);
	}
	item *items = (*head)->next;
	//if(items== NULL) {std::cout<<"failure to launch "<<(head->si)->val<<std::endl; }
	//if(head== NULL) {std::cout<<"failure to launch2"<<std::endl; }
	//std::cout<<(head->si)->val<<std::endl;;
	for (int i = 0; i < NUM_OPS; i++) {
		//for(int y = 0; y < num_sub_items; y++){
			rv = fprintf(file,"%d ",(items->si)->val);
		//}
		rv2 = fprintf(file,"\n");

		if(rv < 0 || rv2<0) {
			printf("ERROR: Unable to write to file `%s'.\n", outputFile);
			fclose(file);
			exit(1);
		}

		if(items->next != NULL) {items = items->next; }
		else {
			std::cout<<"i is: "<<i<<std::endl;
			return;}
	}

	rv = fclose(file);
	if(rv != 0) {
		printf("ERROR: Unable to close file `%s'.\n", outputFile);
		exit(1);
	}
}

void concurrent_queue::write_to_file(std::string string, char* output)
{
	int i, rv,rv2;
	FILE *file;
	char *outputFile;
	//if (argo::node_id() == 0) {
		if(output == NULL) {
			outputFile = (char*)"out.txt";
		}
		else {
			outputFile =output;
		}

	file = fopen(outputFile, "a");
	if(file == NULL) {
		printf("ERROR: Unable to open file `%s'.\n", outputFile);
		exit(1);
	}
	rv = fprintf(file,"%s",string.c_str());
	if(rv < 0) {
			printf("ERROR: Unable to write to file `%s'.\n", outputFile);
			fclose(file);
			exit(1);
		}
	rv = fclose(file);
	if(rv != 0) {
		printf("ERROR: Unable to close file `%s'.\n", outputFile);
		exit(1);
	}
}
int concurrent_queue::returnHead() {
	return ((*head)->si)->val;
}
item* concurrent_queue::returnNext() {
	return (*head)->next;
}



//End of verification functions

/******************************************
 * tail-> item2 <- item1 <- item0 <- head *
 *        next<-item               *
 *     push > tail...head > pop      *
 ******************************************/

void concurrent_queue::push(int val) {
	item *new_item = argo::new_<item>();

	enq_lock->lock(); //todo change to collective coherence
	#if SELECTIVE_ACQREL
			argo::backend::selective_acquire(tail, sizeof(item*));
	#endif
	for (int i = 0; i < num_sub_items; i++) {
		(new_item->si + i)->val = val;
	}
	new_item->next = NULL;
	(*tail)->next = new_item;
	*tail = new_item;
	#if SELECTIVE_ACQREL
		argo::backend::selective_release(tail, sizeof(item*));
	#endif
	enq_lock->unlock();
}

bool concurrent_queue::pop(int &out) {
	deq_lock->lock();		//#todo change to selective coherence
	#if SELECTIVE_ACQREL
			argo::backend::selective_acquire(head, sizeof(item*));
	#endif
	item *node = *head;
	item *new_head = node->next;
	if (new_head == NULL) {
		deq_lock->unlock();
		return false;
	}
	out = (new_head->si)->val;
	*head = new_head;
	#if SELECTIVE_ACQREL
			argo::backend::selective_release(head, sizeof(item*));
	#endif
	deq_lock->unlock();

	argo::delete_(node);
	return true;
}

void concurrent_queue::init() {
	item *new_item = argo::new_<item>();
	for (int i = 0; i < num_sub_items; i++) {
		(new_item->si + i)->val = -1;
	}
	new_item->next = NULL;
	*head = new_item;
	*tail = new_item;
}

void concurrent_queue::check() {
	std::size_t checksum = 0;
	for (item* check = *head; check != nullptr; check = check->next) {
		for (int i = 0; i < num_sub_items; i++) {
			checksum += (check->si + i)->val;
		}
	}

	std::size_t expectsum = 0;
	std::size_t nodechunk = NUM_OPS / argo::number_of_nodes();
	for (int i = 0; i < argo::number_of_nodes(); ++i) {
		expectsum += (9+i) * nodechunk*num_sub_items;
	}
	expectsum -= num_sub_items;
	assert(checksum == expectsum);
	std::cout<<"Verification successful!"<<std::endl;
}

concurrent_queue::concurrent_queue() {
	head = argo::conew_<item*>();
	tail = argo::conew_<item*>();
	#if SELECTIVE_ACQREL
		enq_lock = new argo::globallock::cohort_lock(true);
		deq_lock = new argo::globallock::cohort_lock(true);
	#else
		enq_lock = new argo::globallock::cohort_lock();
		deq_lock = new argo::globallock::cohort_lock();
	#endif
	num_sub_items = NUM_SUB_ITEMS;
}

concurrent_queue::~concurrent_queue() {
	int temp;
	/*
	#if DEBUG
		int* a = argo::conew_<int>(0);
		while(pop(temp)) {
			enq_lock->lock(); //Uses first lock since its not used at this point in code to provide atomicity to addition for variable a.
			#if SELECTIVE_ACQREL
					argo::backend::selective_acquire(a, sizeof(int));
			#endif
				*a += temp;
			#if SELECTIVE_ACQREL
				argo::backend::selective_release(a, sizeof(int));
			#endif
			enq_lock->unlock();
		}
		std::size_t expectsum = 0;
		std::size_t nodechunk = NUM_OPS / argo::number_of_nodes();
		for (int i = 0; i < argo::number_of_nodes(); ++i) {
			expectsum += (9+i) * nodechunk*num_sub_items;
		}
		argo::barrier();

		assert((*a)*num_sub_items == expectsum);
		std::cout<<"Deconstructor verification successful at node: "<<argo::node_id() <<std::endl;
	#else */
 		while(pop(temp));
	//#endif

	delete enq_lock;
	delete deq_lock;
	argo::codelete_(head);
	argo::codelete_(tail);
}
