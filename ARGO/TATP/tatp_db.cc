/*
Author: Vaibhav Gogte <vgogte@umich.edu>
Aasheesh Kolli <akolli@umich.edu>

ArgoDSM/PThreads version:
Ioannis Anevlavis <ioannis.anevlavis@etascale.com>

This file defines the various transactions in TATP.
*/

#include "tatp_db.h"

#include <cstdlib>
#include <iostream>

extern int workrank;
extern int numtasks;




int getRand() {
	return rand();
}

void distribute(int& beg,
		int& end,
		const int& loop_size,
		const int& beg_offset,
    		const int& less_equal){
	int chunk = loop_size / numtasks;
	beg = workrank * chunk + ((workrank == 0) ? beg_offset : less_equal);
	end = (workrank != numtasks - 1) ? workrank * chunk + chunk : loop_size;
}

TATP_DB::TATP_DB(unsigned num_subscribers) {}

//#changed
//Start: Functions meant for debugging and verification purposes.

//Subscriber entries
void TATP_DB::printTotalSubscribers() { printf("%lu printing subs\n",total_subscribers); }
void TATP_DB::printdbTable() {
	for(int i = 0; i<total_subscribers; i++ ) {
		//printf("Printing subscriber id of element %d:%u\n",i, subscriber_table[i].s_id); //Will be the same as index, printing not necessary.
		printf("Printing vlr_location of element %d:%u\n",i, subscriber_table[i].vlr_location);
		printf("Printing subscriber number of entry %d:", i);
		for (int j=0; j<15; j++) {
			printf("%d",subscriber_table[i].sub_nbr[j]);//Is never set to printing is not necessary.
		}
		printf("\n");
		//Padding not specified
		/*printf("\n Printing padding of element %d:",i);
		for (int k=0; k<5; k++) {
			printf("%c", subscriber_table[i].padding[k]);
		}
		printf("\n"); */
	}
}
void TATP_DB::write_to_file()
{
	int i, rv,rv2;
	FILE *file;
	char *outputFile = (char*)"out.txt";

	file = fopen(outputFile, "w");
	if(file == NULL) {
		printf("ERROR: Unable to open file `%s'.\n", outputFile);
		exit(1);
	}
	for(int i = 0; i<total_subscribers; i++ ) {
		rv = fprintf(file,"Printing vlr_location of element %d:%u Subscriber number:", i, subscriber_table[i].vlr_location);
		for (int j=0; j<15; j++) {
			rv2 = fprintf(file,"%d",subscriber_table[i].sub_nbr[j]);//Is never set to printing is not necessary.
		}
		fprintf(file,"\n");

		if(rv < 0 || rv2<0) {
			printf("ERROR: Unable to write to file `%s'.\n", outputFile);
			fclose(file);
			exit(1);
		}
	}
	rv = fclose(file);
	if(rv != 0) {
		printf("ERROR: Unable to close file `%s'.\n", outputFile);
		exit(1);
	}
}

//access_info_table
void TATP_DB::printAI_table() {
	for(int i=0; i<total_subscribers; i++) {
		printf("Printing ai_type for element %d: %d\n", i, access_info_table[i].ai_type);
	}
}


//special_facility_table
void TATP_DB::printSF_table() {
	for(int i=0; i<4*total_subscribers; i++) {
		if(special_facility_table[i].valid==1) {
			printf("Printing s_id for element %d: %d\n", i, special_facility_table[i].s_id);
			printf("Printing sf_type for element %d: %d\n", i, special_facility_table[i].sf_type);
			printf("Printing is_active for element %d: %d\n",i,  special_facility_table[i].is_active);
			printf("Printing error_cntrl for element %d: %d\n",i,  special_facility_table[i].error_cntrl);
			printf("Printing data_a for element %d: %d\n",i,  special_facility_table[i].data_a);
			printf("Printing data_b for element %d: ", i);
			for (int j=0; j<5; j++) {
				printf("%c",special_facility_table[i].data_b[j]);//Is never set to printing is not necessary.
			}
			printf("\n");
			printf("Printing valid for element %d: %d\n",i, special_facility_table[i].valid);
			printf("\n");
		}
	}
}



//End: Functions meant for debugging and verification purposes.




void TATP_DB::initialize(unsigned num_subscribers, int n) {
	total_subscribers = num_subscribers;
	num_threads = n;
	subscriber_table = argo::conew_array<subscriber_entry>(num_subscribers);

	// A max of 4 access info entries per subscriber
	access_info_table = argo::conew_array<access_info_entry>(4*num_subscribers);

	// A max of 4 access info entries per subscriber
	special_facility_table = argo::conew_array<special_facility_entry>(4*num_subscribers);

	// A max of 3 call forwarding entries per "special facility entry"
	call_forwarding_table= argo::conew_array<call_forwarding_entry>(3*4*num_subscribers);

	#if MOD_ARGO
		//argo::globallock::global_tas_lock is equivelant to global_lock_type at line 47 in synchronization/cohort_lock.hpp
		//argo::globallock::global_tas_lock::internal_field_type* lockptrs = argo::conew_array<argo::globallock::global_tas_lock::internal_field_type>(num_subscribers);
		lockptrs = argo::conew_array<argo::globallock::global_tas_lock::internal_field_type>(num_subscribers);
	#endif
	lock_ = new argo::globallock::cohort_lock*[num_subscribers]; //#todo lock created, change to non sync?

	for(int i=0; i<num_subscribers; i++) {
		#if SELECTIVE_ACQREL
			#if !MOD_ARGO
				lock_[i] = new argo::globallock::cohort_lock(true); //#changed Changing to non sync locking.
			#else
				lock_[i] = new argo::globallock::cohort_lock(&lockptrs[i],true); //#changed Changing to non sync locking.
			#endif
		#else
			#if !MOD_ARGO
				lock_[i] = new argo::globallock::cohort_lock();
			#else
				lock_[i] = new argo::globallock::cohort_lock(&lockptrs[i]);
			#endif
		#endif
	}
	//delete[] lockptrs;
	int beg, end;
	distribute(beg, end, 4*num_subscribers, 0, 0);

	for(int i=beg; i<end; i++) {
		access_info_table[i].valid = false;
		special_facility_table[i].valid = false;
		for(int j=0; j<3; j++) {
			call_forwarding_table[3*i+j].valid = false;
		}
	}

	//rndm_seeds = new std::atomic<unsigned long>[NUM_RNDM_SEEDS];
	//rndm_seeds = (std::atomic<unsigned long>*) malloc(NUM_RNDM_SEEDS*sizeof(std::atomic<unsigned long>));
	subscriber_rndm_seeds = new unsigned long[NUM_RNDM_SEEDS];
	vlr_rndm_seeds = new unsigned long[NUM_RNDM_SEEDS];
	rndm_seeds = new unsigned long[NUM_RNDM_SEEDS];
	//sgetRand();
	for(int i=0; i<NUM_RNDM_SEEDS; i++) {
		subscriber_rndm_seeds[i] = getRand()%(NUM_RNDM_SEEDS*10)+1;
		vlr_rndm_seeds[i] = getRand()%(NUM_RNDM_SEEDS*10)+1;
		rndm_seeds[i] = getRand()%(NUM_RNDM_SEEDS*10)+1;
		//std::cout<<i<<" "<<rndm_seeds[i]<<std::endl;
	}
}

TATP_DB::~TATP_DB(){
	for(int i=0; i<total_subscribers; i++) { //#todo maybe change deconstructor for new locks?
		delete lock_[i];
	}
	delete[] lock_;
	delete[] subscriber_rndm_seeds;
	delete[] vlr_rndm_seeds;
	delete[] rndm_seeds;
	argo::codelete_array(lockptrs);
	argo::codelete_array(subscriber_table);
	argo::codelete_array(access_info_table);
	argo::codelete_array(special_facility_table);
	argo::codelete_array(call_forwarding_table);
}

void TATP_DB::populate_tables(unsigned num_subscribers) {
	int beg, end;
	distribute(beg, end, num_subscribers, 0, 0);

	for(int i=beg; i<end; i++) {
		fill_subscriber_entry(i);
		int num_ai_types = getRand()%4 + 1; // num_ai_types varies from 1->4
		for(int j=1; j<=num_ai_types; j++) {
			fill_access_info_entry(i,j);
		}
		int num_sf_types = getRand()%4 + 1; // num_sf_types varies from 1->4
		for(int k=1; k<=num_sf_types; k++) {
			fill_special_facility_entry(i,k);
			int num_call_forwards = getRand()%4; // num_call_forwards varies from 0->3
			for(int p=0; p<num_call_forwards; p++) {
				fill_call_forwarding_entry(i,k,8*p);
			}
		}
	}
	argo::barrier();
}

void TATP_DB::fill_subscriber_entry(unsigned _s_id) {
	subscriber_table[_s_id].s_id = _s_id;
	convert_to_string(_s_id, 15, subscriber_table[_s_id].sub_nbr);

	subscriber_table[_s_id].bit_1 = (short) (getRand()%2);
	subscriber_table[_s_id].bit_2 = (short) (getRand()%2);
	subscriber_table[_s_id].bit_3 = (short) (getRand()%2);
	subscriber_table[_s_id].bit_4 = (short) (getRand()%2);
	subscriber_table[_s_id].bit_5 = (short) (getRand()%2);
	subscriber_table[_s_id].bit_6 = (short) (getRand()%2);
	subscriber_table[_s_id].bit_7 = (short) (getRand()%2);
	subscriber_table[_s_id].bit_8 = (short) (getRand()%2);
	subscriber_table[_s_id].bit_9 = (short) (getRand()%2);
	subscriber_table[_s_id].bit_10 = (short) (getRand()%2);

	subscriber_table[_s_id].hex_1 = (short) (getRand()%16);
	subscriber_table[_s_id].hex_2 = (short) (getRand()%16);
	subscriber_table[_s_id].hex_3 = (short) (getRand()%16);
	subscriber_table[_s_id].hex_4 = (short) (getRand()%16);
	subscriber_table[_s_id].hex_5 = (short) (getRand()%16);
	subscriber_table[_s_id].hex_6 = (short) (getRand()%16);
	subscriber_table[_s_id].hex_7 = (short) (getRand()%16);
	subscriber_table[_s_id].hex_8 = (short) (getRand()%16);
	subscriber_table[_s_id].hex_9 = (short) (getRand()%16);
	subscriber_table[_s_id].hex_10 = (short) (getRand()%16);

	subscriber_table[_s_id].byte2_1 = (short) (getRand()%256);
	subscriber_table[_s_id].byte2_2 = (short) (getRand()%256);
	subscriber_table[_s_id].byte2_3 = (short) (getRand()%256);
	subscriber_table[_s_id].byte2_4 = (short) (getRand()%256);
	subscriber_table[_s_id].byte2_5 = (short) (getRand()%256);
	subscriber_table[_s_id].byte2_6 = (short) (getRand()%256);
	subscriber_table[_s_id].byte2_7 = (short) (getRand()%256);
	subscriber_table[_s_id].byte2_8 = (short) (getRand()%256);
	subscriber_table[_s_id].byte2_9 = (short) (getRand()%256);
	subscriber_table[_s_id].byte2_10 = (short) (getRand()%256);

	subscriber_table[_s_id].msc_location = getRand()%(2^32 - 1) + 1;
	#if ENABLE_VERIFICATION == 1
		subscriber_table[_s_id].vlr_location = 0; //#changed for verification purposes. #Verification
	#else
		subscriber_table[_s_id].vlr_location = getRand()%(2^32 - 1) + 1;
	#endif
	//subscriber_table[_s_id].vlr_location = getRand()%(2^32 - 1) + 1;
}

void TATP_DB::fill_access_info_entry(unsigned _s_id, short _ai_type) {

	int tab_indx = 4*_s_id + _ai_type - 1;

	access_info_table[tab_indx].s_id = _s_id;
	access_info_table[tab_indx].ai_type = _ai_type;

	access_info_table[tab_indx].data_1 = getRand()%256;
	access_info_table[tab_indx].data_2 = getRand()%256;

	make_upper_case_string(access_info_table[tab_indx].data_3, 3);
	make_upper_case_string(access_info_table[tab_indx].data_4, 5);

	access_info_table[tab_indx].valid = true;
}

void TATP_DB::fill_special_facility_entry(unsigned _s_id, short _sf_type) {

	int tab_indx = 4*_s_id + _sf_type - 1;

	special_facility_table[tab_indx].s_id = _s_id;
	special_facility_table[tab_indx].sf_type = _sf_type;

	special_facility_table[tab_indx].is_active = ((getRand()%100 < 85) ? 1 : 0);

	special_facility_table[tab_indx].error_cntrl = getRand()%256;
	special_facility_table[tab_indx].data_a = getRand()%256;

	make_upper_case_string(special_facility_table[tab_indx].data_b, 5);
	special_facility_table[tab_indx].valid = true;
}

void TATP_DB::fill_call_forwarding_entry(unsigned _s_id, short _sf_type, short _start_time) {
	if(_start_time == 0)
		return;

	int tab_indx = 12*_s_id + 3*(_sf_type-1) + (_start_time-8)/8;

	call_forwarding_table[tab_indx].s_id = _s_id;
	call_forwarding_table[tab_indx].sf_type = _sf_type;
	call_forwarding_table[tab_indx].start_time = _start_time - 8;

	call_forwarding_table[tab_indx].end_time = (_start_time - 8) + getRand()%8 + 1;

	convert_to_string(getRand()%1000, 15, call_forwarding_table[tab_indx].numberx);
}

void TATP_DB::convert_to_string(unsigned number, int num_digits, char* string_ptr) {

	char digits[10] = {'0','1','2','3','4','5','6','7','8','9'};
	int quotient = number;
	int reminder = 0;
	int num_digits_converted=0;
	int divider = 1;
	while((quotient != 0) && (num_digits_converted<num_digits)) {
		divider = 10^(num_digits_converted+1);
		reminder = quotient%divider; quotient = quotient/divider;
		string_ptr[num_digits-1 - num_digits_converted] = digits[reminder];
		num_digits_converted++;
	}
	if(num_digits_converted < num_digits) {
		string_ptr[num_digits-1 - num_digits_converted] = digits[0];
		num_digits_converted++;
	}
	return;
}

void TATP_DB::make_upper_case_string(char* string_ptr, int num_chars) {
	char alphabets[26] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N',
		'O','P','Q','R','S','T','U','V','W','X','Y','Z'};
	for(int i=0; i<num_chars; i++) {
		string_ptr[i] = alphabets[getRand()%26];
	}
	return;
}

void TATP_DB::update_subscriber_data(int threadId) {
	unsigned rndm_s_id = getRand() % total_subscribers;
	short rndm_sf_type = getRand() % 4 + 1;
	unsigned special_facility_tab_indx = 4*rndm_s_id + rndm_sf_type -1;

	if(special_facility_table[special_facility_tab_indx].valid) {

		//FIXME: There is a potential data race here, do not use this function yet
		lock_[rndm_s_id]->lock();
		subscriber_table[rndm_s_id].bit_1 = getRand()%2;
		special_facility_table[special_facility_tab_indx].data_a = getRand()%256;
		lock_[rndm_s_id]->unlock();

	}
	return;
}

void TATP_DB::update_location(int threadId, int num_ops) {
	long rndm_s_id;
	rndm_s_id = get_random_s_id(threadId)-1;
	//rndm_s_id /=total_subscribers; // with this rndm_s_id is always 0
	lock_[rndm_s_id]->lock(); //#changed added nonsynch locks and added specific acq/rel.
	#if SELECTIVE_ACQREL
		argo::backend::selective_acquire(&subscriber_table[rndm_s_id].vlr_location, sizeof(unsigned));
	#endif

	#if ENABLE_VERIFICATION == 1
		subscriber_table[rndm_s_id].vlr_location += 1; //#changed For verification purposes #verification
	#else
		subscriber_table[rndm_s_id].vlr_location = get_random_vlr(threadId);
	#endif

	#if SELECTIVE_ACQREL
		argo::backend::selective_release(&subscriber_table[rndm_s_id].vlr_location, sizeof(unsigned));
	#endif
	lock_[rndm_s_id]->unlock();

	return;
}

//For verification purposes #Verification #changed
void TATP_DB::verify() {
	long acc = 0;
	for (long i = 0; i < total_subscribers; ++i)
		acc += subscriber_table[i].vlr_location;

	long ops = NUM_THREADS*numtasks;
	ops *= NUM_OPS/(NUM_THREADS*numtasks);

	if (acc == ops)
		std::cout << "VERIFICATION: SUCCESS" << std::endl;
	else
		std::cout << "VERIFICATION: FAILURE" <<", Acc: "<<acc<<", ops= "<<ops << std::endl;
}
//end of verification function





void TATP_DB::insert_call_forwarding(int threadId) {
	return;
}

void TATP_DB::delete_call_forwarding(int threadId) {
	return;
}

void TATP_DB::print_results() {
	//std::cout<<"TxType:0 successful txs = "<<txCounts[0][0]<<std::endl;
	//std::cout<<"TxType:0 failed txs = "<<txCounts[0][1]<<std::endl;
	//std::cout<<"TxType:1 successful txs = "<<txCounts[1][0]<<std::endl;
	//std::cout<<"TxType:1 failed txs = "<<txCounts[1][1]<<std::endl;
	//std::cout<<"TxType:2 successful txs = "<<txCounts[2][0]<<std::endl;
	//std::cout<<"TxType:2 failed txs = "<<txCounts[2][1]<<std::endl;
	//std::cout<<"TxType:3 successful txs = "<<txCounts[3][0]<<std::endl;
	//std::cout<<"TxType:3 failed txs = "<<txCounts[3][1]<<std::endl;
}

unsigned long TATP_DB::get_random(int thread_id) {
	//return (getRand()%65536 | min + getRand()%(max - min + 1)) % (max - min + 1) + min;
	unsigned long tmp;
	tmp = rndm_seeds[thread_id*10] = (rndm_seeds[thread_id*10] * 16807) % 2147483647;
	return tmp;
}

unsigned long TATP_DB::get_random(int thread_id, int min, int max) {
	//return (getRand()%65536 | min + getRand()%(max - min + 1)) % (max - min + 1) + min;
	unsigned long tmp;
	tmp = rndm_seeds[thread_id*10] = (rndm_seeds[thread_id*10] * 16807) % 2147483647;
	return (min+tmp%(max-min+1));
}

unsigned long TATP_DB::get_random_s_id(int thread_id) {
	unsigned long tmp;
	tmp = subscriber_rndm_seeds[thread_id*10] = (subscriber_rndm_seeds[thread_id*10] * 16807) % 2147483647;
	return (1 + tmp%(total_subscribers));
}

unsigned long TATP_DB::get_random_vlr(int thread_id) {
	unsigned long tmp;
	tmp = vlr_rndm_seeds[thread_id*10] = (vlr_rndm_seeds[thread_id*10] * 16807)%2147483647;
	return (1 + tmp%(2^32));
}
