//Mengting Nan
//EECS 570 
//Winter 2014 
//Project 1a 
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <sys/time.h>
#include <assert.h>

using namespace std;

int num_of_core, num_of_int;

vector<int> input;	
vector<int> output = input;	
vector<int> output_s = input;	

int min_block_size;
int max_block_size;
int num_of_large_block;

pthread_t *thread;			
char* msg;

void serial_prefixSum(){

	output_s.push_back(input[0]);
	for(int i=1; i<num_of_int; i++){
		int temp = input[i]+output_s[i-1]; 
		output_s.push_back(temp);
	}
	//cout << "The output_s array is: ";
	//for(int i=0; i<num_of_int; i++){
	//	cout << output_s[i] << " "; 
	//}
	//cout << endl;
}

void *sum_bottom_block(void *msg){

	int id_num = *(int*)&msg;
	int idx_adj;
	int block_size;
	//cout << "id_num is " << id_num << endl;

	//This block is a large block
	if(id_num < num_of_large_block){

		//calculate the adjustment for the index
		idx_adj = max_block_size*id_num;
		block_size = max_block_size;
	}

	else{
		idx_adj = max_block_size*num_of_large_block + min_block_size*(id_num-num_of_large_block);
		block_size = min_block_size;
	}

	//calculate the block_output
	for(int i=1; i<block_size; i++){
		int idx = idx_adj+i;
		output[idx] = input[idx]+output[idx-1]; 
	}

	pthread_exit(NULL);
}

void *sum_level_block(void *msg){	

	int id_num = *(int*)&msg + 1;
	int idx_adj=0;
	int block_size;
	int update_bin =0;

	if(id_num < num_of_large_block){

		//calculate the adjustment for the index
		idx_adj = max_block_size*id_num;
		block_size = max_block_size;
		for(int i=1; i<id_num+1; i++){
			update_bin += input[i*max_block_size-1];
		}
	}

	else{
		idx_adj = max_block_size*num_of_large_block + min_block_size*(id_num-num_of_large_block);
		block_size = min_block_size;
		for(int i=1; i<num_of_large_block+1; i++){
			update_bin += input[i*max_block_size-1];
		}
		for(int i=1; i<id_num-num_of_large_block+1; i++){
			update_bin += input[max_block_size*num_of_large_block+i*min_block_size-1];
		}
	}

	//cout << "update_bin is" << update_bin << endl;

	//update the level_output
	for(int i=0; i<block_size; i++){
		int idx = idx_adj+i;
		output[idx] = input[idx]+update_bin;
	}

	pthread_exit(NULL);
}

void sum_per_level(int num_active_cores){

	thread = (pthread_t *) malloc(sizeof(pthread_t) *num_active_cores);

	for(int j=0; j < num_active_cores; j++){

		int rc = pthread_create(&thread[j], NULL, sum_level_block, (void *)j);

		if(rc){
			printf("ERROR: return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
	}
	for(int i=0; i<num_active_cores; i++){
		pthread_join(thread[i], NULL);
	}

	//cout << "The level_output array  is: ";
	//for(int i=0; i<level_output.size(); i++){
	//	cout << level_output[i] << " "; 
	//}
	//cout << endl;
}



void sum_bottom_level(int num_active_cores){

	thread = (pthread_t *) malloc(sizeof(pthread_t) *num_active_cores);

	for(int j=0; j < num_active_cores; j++){

		int rc = pthread_create(&thread[j], NULL, sum_bottom_block, (void *)j);

		if(rc){
			printf("ERROR: return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
	}
	
	for(int i=0; i<num_active_cores; i++){
		pthread_join(thread[i], NULL);
	}

	//cout << "The level_output array  is: ";
	//for(int i=0; i<level_output.size(); i++){
	//	cout << level_output[i] << " "; 
	//}
	//cout << endl;
}

void active_core_update(int &active_core_num, int input_size){

	if (input_size%2 ==0){
		if(active_core_num > input_size/2){
			active_core_num = input_size/2;
		}
	}
	else if(active_core_num > (input_size+1)/2){
		active_core_num = (input_size+1)/2;
	}
	//cout << "active_core_num is: " << active_core_num << endl;
}


void parallel_prefixSum(){

	//Initialize number of active cores
	int input_size = (int) input.size();
	int num_active_cores = num_of_core - 1;

	active_core_update(num_active_cores, input_size);
	min_block_size = input_size/num_active_cores;	
	num_of_large_block = input_size%num_active_cores;
	max_block_size = (num_of_large_block == 0)? min_block_size: min_block_size+1;

	output = input;
	sum_bottom_level(num_active_cores);

	input = output;
	num_active_cores--;
	sum_per_level(num_active_cores);
}

void performance_chk(struct timeval start, struct timeval end){
	long mtime, seconds, useconds;

	seconds = end.tv_sec - start.tv_sec;
	useconds = end.tv_usec - start.tv_usec;
	mtime = ((seconds)*1000000 + useconds);
	printf ("Running time for %s is	: %ld microseconds\n",msg, mtime);	
}


int main(int argc, char* argv[]){

	char* num_of_core_str = argv[1];
	char* num_of_int_str  = argv[2];

	struct timeval start, end;

	istringstream buffer (num_of_core_str);
	buffer >> num_of_core;
	istringstream buffer_1 (num_of_int_str);
	buffer_1 >> num_of_int;

	cout << "=========================================================================================\n";
	cout << "The number of core is: " << num_of_core << "	|	";
	cout << "The number of interger is: " << num_of_int << endl;

	//cout << "The input array is: ";
	for(int i=0; i<num_of_int; i++){

		int temp = rand()%10;	
		input.push_back(temp);
	//	cout << temp << " ";
	}

	gettimeofday(&start, NULL);

	//serial_prefixSum();

	gettimeofday(&end, NULL);
	msg = "serial_prefixSum";
	cout << "----------------------------------------------------------------------------------------\n";
	performance_chk(start, end);

	if(num_of_core > 1){
		gettimeofday(&start, NULL);

		parallel_prefixSum();

		gettimeofday(&end, NULL);
	}
	else
		printf("core number is 1: NOT able to run parallel_prefixSum.\n");

	msg = "parallel_prefixSum";
	cout << "----------------------------------------------------------------------------------------\n";
	performance_chk(start, end);
	cout << "========================================================================================\n";

	assert(output == output_s);
	if(output == output_s)
		cout << "@PASS\n";
	else
		cout << "@FAIL\n";
}
