/*
Author: Vaibhav Gogte <vgogte@umich.edu>
Aasheesh Kolli <akolli@umich.edu>

ArgoDSM/PThreads version:
Ioannis Anevlavis <ioannis.anevlavis@etascale.com>

This file defines the various functions of the tpcc database
*/

#include "tpcc_db.h"

#include <cstring>
#include <cstdlib>

#include <queue>
#include <algorithm>
#include <iostream>

extern int workrank;
extern int numtasks;






void TPCC_DB::verification(){
	int num_stocks = NUM_ITEMS*NUM_WAREHOUSES;
	int tot = 0;
	for(int i = 0; i<num_stocks; i++){
		tot += stock[i].s_order_cnt;
	}
	int expected = 10*NUM_ORDERS;
	assert(tot==expected);

	int num_new_order = 10*NUM_WAREHOUSES*900;
	int total_new_order = 0;
	for(int i = 0; i<num_new_order; i++){
		total_new_order += new_order[i].no_w_id;
	}
	int expected_new_order = num_new_order+NUM_ORDERS;
	assert(total_new_order == expected_new_order);

	int num_update_order = 3000*10*NUM_WAREHOUSES;
	int total_update_order = 0;
	for(int i = 0; i<num_update_order; i++){
		total_update_order += order[i].o_all_local;
	}
	int expected_update_order = NUM_ORDERS+num_update_order;
	assert(total_update_order == expected_update_order);

	std::cout<<"VERIFICATION SUCCESSFUL"<<std::endl;
}
//End of printing function


void distribute(int& beg,
		int& end,
		const int& loop_size,
		const int& beg_offset,
    		const int& less_equal){
	int chunk = loop_size / numtasks;
	beg = workrank * chunk + ((workrank == 0) ? beg_offset : less_equal);
	end = (workrank != numtasks - 1) ? workrank * chunk + chunk : loop_size;
}

TPCC_DB::TPCC_DB() {}

void TPCC_DB::initialize(int _num_warehouses, int numThreads, int numLocks) {
	num_locks = numLocks;
	num_warehouses = _num_warehouses;
	int num_districts = 10*num_warehouses;
	int num_customers = 3000*num_districts;
	int num_stocks = NUM_ITEMS*num_warehouses;

	for(int i=0; i<3000; i++) {
		random_3000[i] = i;
	}
	for(int i=0; i<3000; i++) {
		int rand_loc = std::rand()%3000;
		int temp = random_3000[i];
		random_3000[i] = random_3000[rand_loc];
		random_3000[rand_loc] = temp;
	}

	perTxLocks = new queue_t[numThreads];
	for(int i=0; i<numThreads; i++) {
		perTxLocks[i].push(0);
		perTxLocks[i].pop();
	}
	#if MOD_ARGO
		//argo::globallock::global_tas_lock is equivelant to global_lock_type at line 47 in synchronization/cohort_lock.hpp
		lockptrs = argo::conew_array<argo::globallock::global_tas_lock::internal_field_type>(numLocks);
	#endif
	locks = new argo::globallock::cohort_lock*[numLocks];

	for(int i=0; i<numLocks; i++) {
		#if SELECTIVE_ACQREL == 1
			#if !MOD_ARGO
				locks[i] = new argo::globallock::cohort_lock(true); //#changed
			#else
				locks[i] = new argo::globallock::cohort_lock(&lockptrs[i],true); //#changed
			#endif
		#else
			#if !MOD_ARGO
		 		locks[i] = new argo::globallock::cohort_lock();
			#else
				locks[i] = new argo::globallock::cohort_lock(&lockptrs[i]);
			#endif
		#endif
		//locks[i] = new argo::globallock::cohort_lock();
	}

	WEXEC(std::cout<<"Allocating tables"<<std::endl);

	warehouse = argo::conew_array<warehouse_entry>(num_warehouses);
	district = argo::conew_array<district_entry>(num_districts);
	customer = argo::conew_array<customer_entry>(num_customers);
	stock = argo::conew_array<stock_entry>(num_stocks);

	int num_items = NUM_ITEMS;
	item = argo::conew_array<item_entry>(num_items);

	int num_histories = num_customers;
	int num_orders = 3000*num_districts;
	int num_order_lines = 15*num_orders; // Max possible, average is 10*num_orders
	int num_new_orders = 900*num_districts;

	history = argo::conew_array<history_entry>(num_histories);
	order = argo::conew_array<order_entry>(num_orders);
	new_order = argo::conew_array<new_order_entry>(num_new_orders);
	order_line = argo::conew_array<order_line_entry>(num_order_lines);

	rndm_seeds = new unsigned long[NUM_RNDM_SEEDS];
	for(int i=0; i<NUM_RNDM_SEEDS; i++) {
		srand(i);
		rndm_seeds[i] = rand_local(1,NUM_RNDM_SEEDS*10);
	}

	WEXEC(std::cout<<"Finished allocating tables"<<std::endl);

	WEXEC(std::cout<<"warehouse_entry: "<<sizeof(warehouse_entry)<<std::endl);
	WEXEC(std::cout<<"district_entry: "<<sizeof(district_entry)<<std::endl);
	WEXEC(std::cout<<"customer_entry: "<<sizeof(customer_entry)<<std::endl);
	WEXEC(std::cout<<"stock_entry: "<<sizeof(stock_entry)<<std::endl);
	WEXEC(std::cout<<"item_entry: "<<sizeof(item_entry)<<std::endl);
	WEXEC(std::cout<<"history_entry: "<<sizeof(history_entry)<<std::endl);
	WEXEC(std::cout<<"order_entry: "<<sizeof(order_entry)<<std::endl);
	WEXEC(std::cout<<"new_order_entry: "<<sizeof(new_order_entry)<<std::endl);
	WEXEC(std::cout<<"order_line_entry: "<<sizeof(order_line_entry)<<std::endl);
}

TPCC_DB::~TPCC_DB(){
	delete[] perTxLocks;
	for(int i=0; i<num_locks; i++) {
		delete locks[i];
	}
	delete[] locks;
	delete[] rndm_seeds;
	argo::codelete_array(lockptrs);
	argo::codelete_array(warehouse);
	argo::codelete_array(district);
	argo::codelete_array(customer);
	argo::codelete_array(stock);
	argo::codelete_array(item);
	argo::codelete_array(history);
	argo::codelete_array(order);
	argo::codelete_array(new_order);
	argo::codelete_array(order_line);
}

void TPCC_DB::populate_tables() {
	WEXEC(std::cout<<"Populating item table"<<std::endl);

	int beg, end;
	distribute(beg, end, NUM_ITEMS, 0, 0);

	for(int i=beg; i<end; i++) {
		fill_item_entry(i+1);
	}
	WEXEC(std::cout<<"Finished populating item table"<<std::endl);

	distribute(beg, end, num_warehouses, 0, 0);

	for(int i=beg; i<end; i++) {
		fill_warehouse_entry(i+1);
		for(int j=0; j<NUM_ITEMS; j++) {
			fill_stock_entry(i+1, j+1);
		}
		std::cout<<"Finished populating stock table"<<std::endl;
		for(int j=0; j<10; j++) {
			fill_district_entry(i+1, j+1);
			for(int k=0; k<3000; k++) {
				fill_customer_entry(i+1, j+1, k+1);
				fill_history_entry(i+1, j+1, k+1);
				fill_order_entry(i+1, j+1, k+1);
			}
			for(int k=2100; k<3000; k++) {
				fill_new_order_entry(i+1, j+1, k+1);
			}
		}
	}
	argo::barrier();
}

void TPCC_DB::acquire_locks(int threadId, queue_t &requestedLocks) { //#todo look into if relevant to argo non sync locking.
	// Acquire locks in order.
	int i = -1;
	while(!requestedLocks.empty()) {
		i = requestedLocks.front();
		perTxLocks[threadId].push(i);
		requestedLocks.pop();
		locks[i]->lock();
	}
}

void TPCC_DB::release_locks(int threadId) { //#todo look into if relevant to argo non sync locking.
	// Release locks in order
	int i = -1;
	while(!perTxLocks[threadId].empty()) {
		i = perTxLocks[threadId].front();
		perTxLocks[threadId].pop();
		locks[i]->unlock();
	}
}

void TPCC_DB::fill_item_entry(int _i_id) {
	int indx = (_i_id-1);
	item[indx].i_id = _i_id;
	item[indx].i_im_id = rand_local(1,NUM_ITEMS);
	random_a_string(14,24,item[indx].i_name);
	item[indx].i_price = rand_local(1,100)*(1.0);
	random_a_original_string(26,50,10,item[indx].i_data);
}

void TPCC_DB::fill_warehouse_entry(int _w_id) {
	int indx = (_w_id-1);
	warehouse[indx].w_id = _w_id;
	random_a_string(6,10,warehouse[indx].w_name);
	random_a_string(10,20,warehouse[indx].w_street_1);
	random_a_string(10,20,warehouse[indx].w_street_2);
	random_a_string(10,20,warehouse[indx].w_city);
	random_a_string(2,2,warehouse[indx].w_state);
	random_zip(warehouse[indx].w_zip);
	warehouse[indx].w_tax = (rand_local(0,20))/100.0;
	warehouse[indx].w_ytd = 300000.0;
}

void TPCC_DB::fill_stock_entry(int _s_w_id, int _s_i_id) {
	//std::cout<<"entered fill stock entry: "<<_s_w_id<<", "<<_s_i_id<<std::endl;
	int indx = (_s_w_id-1)*NUM_ITEMS + (_s_i_id-1);
	stock[indx].s_i_id = _s_i_id;
	//std::cout<<"1"<<std::endl;
	stock[indx].s_w_id = _s_w_id;
	//std::cout<<"1"<<std::endl;
	stock[indx].s_quantity = rand_local(10,100);
	//std::cout<<"1"<<std::endl;
	random_a_string(24,24,stock[indx].s_dist_01);
	//std::cout<<"1"<<std::endl;
	random_a_string(24,24,stock[indx].s_dist_02);
	//std::cout<<"1"<<std::endl;
	random_a_string(24,24,stock[indx].s_dist_03);
	//std::cout<<"1"<<std::endl;
	random_a_string(24,24,stock[indx].s_dist_04);
	//std::cout<<"1"<<std::endl;
	random_a_string(24,24,stock[indx].s_dist_05);
	//std::cout<<"1"<<std::endl;
	random_a_string(24,24,stock[indx].s_dist_06);
	//std::cout<<"1"<<std::endl;
	random_a_string(24,24,stock[indx].s_dist_07);
	//std::cout<<"1"<<std::endl;
	random_a_string(24,24,stock[indx].s_dist_08);
	//std::cout<<"1"<<std::endl;
	random_a_string(24,24,stock[indx].s_dist_09);
	//std::cout<<"1"<<std::endl;
	random_a_string(24,24,stock[indx].s_dist_10);
	//std::cout<<"1"<<std::endl;
	stock[indx].s_ytd = 0.0;
	//std::cout<<"1"<<std::endl;
	stock[indx].s_order_cnt = 0.0;
	//std::cout<<"1"<<std::endl;
	stock[indx].s_remote_cnt = 0.0;
	//std::cout<<"1"<<std::endl;
	random_a_original_string(26,50,10,stock[indx].s_data);
	//std::cout<<"exiting fill stock entry: "<<_s_w_id<<", "<<_s_i_id<<std::endl;
}

void TPCC_DB::fill_district_entry(int _d_w_id, int _d_id) {
	int indx = (_d_w_id-1)*10 + (_d_id-1);
	district[indx].d_id = _d_id;
	district[indx].d_w_id = _d_w_id;
	random_a_string(6,10,district[indx].d_name);
	random_a_string(10,20,district[indx].d_street_1);
	random_a_string(10,20,district[indx].d_street_2);
	random_a_string(10,20,district[indx].d_city);
	random_a_string(2,2,district[indx].d_state);
	random_zip(district[indx].d_zip);
	district[indx].d_tax = (rand_local(0,20))/100.0;
	district[indx].d_ytd = 30000.0;
	district[indx].d_next_o_id = 3001;
}

void TPCC_DB::fill_customer_entry(int _c_w_id, int _c_d_id, int _c_id) {
	int indx = (_c_w_id-1)*10*3000 + (_c_d_id-1)*3000 + (_c_id-1);
	customer[indx].c_id = _c_id;
	customer[indx].c_d_id = _c_d_id;
	customer[indx].c_w_id = _c_w_id;
	random_a_string(16,16,customer[indx].c_last); // FIXME: check tpcc manual for exact setting
	customer[indx].c_middle[0] = 'O';
	customer[indx].c_middle[1] = 'E';
	random_a_string(8,16,customer[indx].c_first);
	random_a_string(10,20,customer[indx].c_street_1);
	random_a_string(10,20,customer[indx].c_street_2);
	random_a_string(10,20,customer[indx].c_city);
	random_a_string(2,2,customer[indx].c_state);
	random_zip(customer[indx].c_zip);
	random_n_string(16,16, customer[indx].c_phone);
	fill_time(customer[indx].c_since);
	if(std::rand()%10 < 1) {
		customer[indx].c_credit[0] = 'G';
		customer[indx].c_credit[1] = 'C';
	}
	else {
		customer[indx].c_credit[0] = 'B';
		customer[indx].c_credit[1] = 'C';
	}
	customer[indx].c_credit_lim = 50000.0;
	customer[indx].c_discount = (rand_local(0,50))/100.0;
	customer[indx].c_balance = -10.0;
	customer[indx].c_ytd_payment = 10.0;
	customer[indx].c_payment_cnt = 1.0;
	customer[indx].c_delivery_cnt = 0.0;
	random_a_string(300,500,customer[indx].c_data);
}

void TPCC_DB::fill_history_entry(int _h_c_w_id, int _h_c_d_id, int _h_c_id) {
	int indx = (_h_c_w_id-1)*10*3000 + (_h_c_d_id-1)*3000 + (_h_c_id-1);
	history[indx].h_c_id = _h_c_id;
	history[indx].h_c_d_id = _h_c_d_id;
	history[indx].h_c_w_id = _h_c_w_id;
	fill_time(history[indx].h_date);
	history[indx].h_amount = 10.0;
	random_a_string(12,24,history[indx].h_data);
}

void TPCC_DB::fill_order_entry(int _o_w_id, int _o_d_id, int _o_id) {
	int indx = (_o_w_id-1)*10*3000 + (_o_d_id-1)*3000 + (_o_id-1);
	order[indx].o_id = _o_id;
	order[indx].o_c_id = random_3000[_o_id];
	order[indx].o_d_id = _o_d_id;
	order[indx].o_w_id = _o_w_id;
	fill_time(order[indx].o_entry_d);
	if(_o_id<2101)
		order[indx].o_carrier_id = std::rand()%10 + 1;
	else
		order[indx].o_carrier_id = 0;
	order[indx].o_ol_cnt = rand_local(5,15);
	order[indx].o_all_local = 1.0;
	for(int i=0; i<order[indx].o_ol_cnt; i++) {
		fill_order_line_entry(_o_w_id, _o_d_id, _o_id, i, order[indx].o_entry_d);
	}
}

void TPCC_DB::fill_order_line_entry(int _ol_w_id, int _ol_d_id, int _ol_o_id, int _o_ol_cnt, long long _o_entry_d) {
	int indx = (_ol_w_id-1)*10*3000*15 + (_ol_d_id-1)*3000*15 + (_ol_o_id-1)*15 + _o_ol_cnt;
	order_line[indx].ol_o_id = _ol_o_id;
	order_line[indx].ol_d_id = _ol_d_id;
	order_line[indx].ol_w_id = _ol_w_id;
	order_line[indx].ol_number = _o_ol_cnt;
	order_line[indx].ol_i_id = rand_local(1,NUM_ITEMS);
	order_line[indx].ol_supply_w_id = _ol_w_id;
	if(_ol_o_id < 2101) {
		order_line[indx].ol_delivery_d = _o_entry_d;
		order_line[indx].ol_amount = 0.0;
	}
	else {
		order_line[indx].ol_delivery_d = 0;
		order_line[indx].ol_amount = rand_local(1,999999)/100.0;
	}
	order_line[indx].ol_quantity = 5.0;
	random_a_string(24,24,order_line[indx].ol_dist_info);
}

void TPCC_DB::fill_new_order_entry(int _no_w_id, int _no_d_id, int _no_o_id) { //Used outside of parallel threads during setup.
	int indx = (_no_w_id-1)*10*900 + (_no_d_id-1)*900 + (_no_o_id-2101) % 900;
	new_order[indx].no_o_id = _no_o_id;
	new_order[indx].no_d_id = _no_d_id;
	new_order[indx].no_w_id = _no_w_id;
}

void TPCC_DB::fill_new_order_entry_CS(int _no_w_id, int _no_d_id, int _no_o_id) { //Used exclusively in critical section as it implements selective_acquire/selective_release.
	int indx = (_no_w_id-1)*10*900 + (_no_d_id-1)*900 + (_no_o_id-2101) % 900;

	#if SELECTIVE_ACQREL == 1
			argo::backend::selective_acquire(&new_order[indx], sizeof(new_order_entry));
	#endif

	new_order[indx].no_o_id = _no_o_id;
	new_order[indx].no_d_id = _no_d_id;
	#if TPCC_DEBUG
		new_order[indx].no_w_id += 	1;
	#else
		new_order[indx].no_w_id = _no_w_id;
	#endif

	#if SELECTIVE_ACQREL == 1
			argo::backend::selective_release(&new_order[indx], sizeof(new_order_entry));
	#endif
}


int TPCC_DB::rand_local(int min, int max) {
	return (min + (std::rand()%(max-min+1)));
}

void TPCC_DB::random_a_string(int min, int max, char* string_ptr) {
	//std::cout<<"entered random a string"<<std::endl;
	char alphabets[26] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N',
		'O','P','Q','R','S','T','U','V','W','X','Y','Z'};
	//std::cout<<"2"<<std::endl;
	int string_length = min + (std::rand()%(max-min+1));
	//std::cout<<"2"<<std::endl;
	for(int i=0; i<string_length; i++) {
		string_ptr[max-1-i] = alphabets[std::rand()%26];
		//std::cout<<"f3"<<std::endl;
	}
	//std::cout<<"2"<<std::endl;
	for(int i=0; i<max-string_length; i++) {
		string_ptr[max-1-i] = ' ';
		//std::cout<<"f4"<<std::endl;
	}
	//std::cout<<"exiting random a string"<<std::endl;
}

void TPCC_DB::random_a_original_string(int min, int max, int probability, char* string_ptr) {
	//FIXME: use probability and add ORIGINAL
	random_a_string(min, max,string_ptr);
}

void TPCC_DB::random_zip(char* string_ptr) {
	random_a_string(4,4,string_ptr);
	for(int i=4; i<9; i++) {
		string_ptr[i] = '1';
	}
}

void TPCC_DB::random_n_string(int min, int max, char* string_ptr) {
	char digits[10] = {'0','1','2','3','4','5','6','7','8','9'};
	int string_length = min + (std::rand()%(max-min+1));
	for(int i=0; i<string_length; i++) {
		string_ptr[max-1-i] = digits[std::rand()%10];
	}
	for(int i=0; i<max-string_length; i++) {
		string_ptr[max-1-i] = ' ';
	}
}

void TPCC_DB::fill_time(long long &time_slot) {
	//FIXME: put correct time
	time_slot = 12112342433241;
}

void TPCC_DB::copy_district_info(district_entry &dest, district_entry &source) {
	std::memcpy(&dest, &source, sizeof(district_entry));
}

void TPCC_DB::copy_customer_info(customer_entry &dest, customer_entry &source) {
	std::memcpy(&dest, &source, sizeof(customer_entry));
}

void TPCC_DB::copy_new_order_info(new_order_entry &dest, new_order_entry &source) {
	std::memcpy(&dest, &source, sizeof(new_order_entry));
}

void TPCC_DB::copy_order_info(order_entry &dest, order_entry &source) {
	std::memcpy(&dest, &source, sizeof(order_entry));
}

void TPCC_DB::copy_stock_info(stock_entry &dest, stock_entry &source) {
	std::memcpy(&dest, &source, sizeof(stock_entry));
}

void TPCC_DB::copy_order_line_info(order_line_entry &dest, order_line_entry &source) {
	std::memcpy(&dest, &source, sizeof(order_line_entry));
}

void TPCC_DB::update_order_entry(int _w_id, short _d_id, int _o_id, int _c_id, int _ol_cnt) {
	int indx = (_w_id-1)*10*3000 + (_d_id-1)*3000 + (_o_id-1)%3000;
	#if SELECTIVE_ACQREL == 1
			argo::backend::selective_acquire(&order[indx], sizeof(order_entry));
	#endif

	order[indx].o_id = _o_id;
	order[indx].o_carrier_id = 0;
	#if TPCC_DEBUG
		order[indx].o_all_local += 1;
	#else
		order[indx].o_all_local = 1;
	#endif
	order[indx].o_ol_cnt = _ol_cnt;
	order[indx].o_c_id = _c_id;
	fill_time(order[indx].o_entry_d);

	#if SELECTIVE_ACQREL == 1
			argo::backend::selective_release(&order[indx], sizeof(order_entry));
	#endif

}


void TPCC_DB::update_stock_entry(int threadId, int _w_id, int _i_id, int _d_id, float &amount) {
	int indx = (_w_id-1)*NUM_ITEMS + _i_id-1;
	//int ol_quantity = get_random(threadId, 1, 10);
	int ol_quantity = 7;
	#if SELECTIVE_ACQREL == 1
			argo::backend::selective_acquire(&stock[indx],sizeof(stock_entry));
			argo::backend::selective_acquire(&item[_i_id-1].i_price, sizeof(float));
	#endif


	if(stock[indx].s_quantity - ol_quantity > 10) {
		stock[indx].s_quantity -= ol_quantity;
	}
	else {
		stock[indx].s_quantity -= ol_quantity;
		stock[indx].s_quantity += 91;
	}

	stock[indx].s_ytd += ol_quantity;
	stock[indx].s_order_cnt += 1;


	amount += ol_quantity * item[_i_id-1].i_price;

	#if SELECTIVE_ACQREL == 1
			argo::backend::selective_release(&stock[indx],sizeof(stock_entry));
	#endif
}

void TPCC_DB::new_order_tx(int threadId, int w_id, int d_id, int c_id) {
	int w_indx = (w_id-1);
	int d_indx = (w_id-1)*10 + (d_id-1);
	int c_indx = (w_id-1)*10*3000 + (d_id-1)*3000 + (c_id-1);


	queue_t reqLocks;
	reqLocks.push(d_indx); // Lock for district

	#if TPCC_DEBUG
		int ol_cnt = 10;
	#else
		int ol_cnt = get_random(threadId, 5, 15);
	#endif
	int item_ids[ol_cnt];
	for(int i=0; i<ol_cnt; i++) {
		int new_item_id;
		bool match;
		do {
			match = false;
			new_item_id = get_random(threadId, 1, NUM_ITEMS);
			for(int j=0; j<i; j++) {
				if(new_item_id == item_ids[j]) {
					match = true;
					break;
				}
			}
		} while (match);
		item_ids[i] = new_item_id;
	}


	std::sort(item_ids, item_ids+ol_cnt);

	for(int i=0; i<ol_cnt; i++) {
		int item_lock_id = num_warehouses*10 + (w_id-1)*NUM_ITEMS + item_ids[i] - 1;

		reqLocks.push(item_lock_id); // Lock for each item in stock table
	}

	acquire_locks(threadId, reqLocks);


	#if SELECTIVE_ACQREL == 1
		argo::backend::selective_acquire(&warehouse[w_indx].w_tax, sizeof(float));
		argo::backend::selective_acquire(&district[d_indx],sizeof(district_entry));
	#endif

	float w_tax = warehouse[w_indx].w_tax;

	float d_tax = district[d_indx].d_tax;
	int d_o_id = district[d_indx].d_next_o_id;

	int no_indx = (w_id-1)*10*900 + (d_id-1)*900 + (d_o_id-2101) % 900;

	int o_indx = (w_id-1)*10*3000 + (d_id-1)*3000 + (d_o_id-1)%3000;

	district[d_indx].d_next_o_id++;

	#if SELECTIVE_ACQREL == 1
		argo::backend::selective_release(&district[d_indx].d_next_o_id, sizeof(int));
	#endif


	fill_new_order_entry_CS(w_id,d_id,d_o_id);
	update_order_entry(w_id, d_id, d_o_id, c_id, ol_cnt);


	float total_amount = 0.0;

	for(int i=0; i<ol_cnt; i++) {
		update_stock_entry(threadId, w_id, item_ids[i], d_id, total_amount);
	}


	release_locks(threadId);
	return;
}

unsigned long TPCC_DB::get_random(int thread_id) {
	unsigned long tmp;
	tmp = rndm_seeds[thread_id*10] = (rndm_seeds[thread_id*10] * 16807) % 2147483647;
	return rand()%(2^32-1);
}

unsigned long TPCC_DB::get_random(int thread_id, int min, int max) {
	unsigned long tmp;
	return min+(rand()%(max-min+1));
}

void TPCC_DB::printStackPointer(int* sp, int thread_id) {
	std::cout<<"Stack Heap: "<<sp<<std::endl;
}
