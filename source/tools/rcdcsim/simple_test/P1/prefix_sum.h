//
//  P1.h
//  P1
//
//  Created by Fiona Yang on 1/14/14.
//  Copyright (c) 2014 University of Michigan. All rights reserved.
//

#ifndef P1_P1_h
#define P1_P1_h

#include <iostream>
#include <vector>
#include <math.h>
#include <assert.h>
#include <pthread.h>

using namespace std;

vector<int> out_parallel;
vector<int> bases;
int n_size;
int core_num;
pthread_mutex_t partial_sum_lock;

void *cal_cycle_1(void * message);
void *cal_cycle_2(void * message);

void print_vector(vector<int> * out_parallel);

vector<int> serial_prefixSum(vector<int> n){
    vector<int> out_serial(n);
    out_serial[0] = n[0];
    for (unsigned int i = 1; i < n.size(); i++) {
        out_serial[i] = out_serial[i-1] + n[i];
        //cout<<"S"<<i + 1 <<" = "<<out_serial[i]<<endl;
    }
    
    return out_serial;
}

vector<int> parallel_prefixSum(vector<int> n, int c_num){
    out_parallel.resize(n.size());
    n_size = (int) n.size();
    
    out_parallel = n;
    
    if (n.size() < (unsigned) 2*c_num){
        core_num = (int) n.size() / 2;
    }
    else{
        core_num = c_num;
    }
    
    bases.resize(core_num);
    vector<int> temp(core_num);
    pthread_t *threads;
    pthread_attr_t attr;
    
    threads = (pthread_t *) malloc(sizeof(pthread_t) * core_num);
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    int ret;
    
    for (int i = 0; i < core_num; i++){
        temp[i] = i;
        ret = pthread_create(&threads[i], &attr, &cal_cycle_1, (void*)(intptr_t) i);
        //cout<<"Cycle 1 Thread created"<<endl;
        assert(ret == 0);
    }
    
    /* wait for all threads to complete */
    //cout<<"Start join"<<endl;

	for(int i = 0; i < core_num; i++){
		ret = pthread_join(threads[i], NULL);
		assert(ret == 0);
	}
    //print_vector(&out_parallel);
    for (int i = 0; i < core_num - 1; i++){
        temp[i] = i;
        ret = pthread_create(&threads[i], &attr, &cal_cycle_2, (void*)(intptr_t) i);
        //cout<<"Cycle 2 Thread created"<<endl;
        assert(ret == 0);
    }
    
    /* wait for all threads to complete */
    //cout<<"Start join"<<endl;
    
	for(int i = 0; i < core_num - 1; i++){
		ret = pthread_join(threads[i], NULL);
		assert(ret == 0);
	}
    
    //print_vector(&out_parallel);
    
    return out_parallel;
}

void *cal_cycle_1(void * message){
    int core_id = (int)(intptr_t) message;
    int start_num = core_id * ceil((double) n_size / core_num);
    int end_num = (core_id + 1) * ceil((double) n_size / core_num);
    int i;
    
    for (i = start_num + 1; i < end_num && i < n_size; i++){
        out_parallel[i] = out_parallel[i-1] + out_parallel[i];
    }
    
    bases[core_id] = out_parallel[i-1];
    
    pthread_exit(NULL);
}

void *cal_cycle_2(void * message){
    int core_id = (int)(intptr_t) message;
    int start_num = (core_id + 1) * ceil((double) n_size / core_num);
    int end_num = (core_id + 2) * ceil((double) n_size / core_num);
    int base = bases[0];
    
    for (int i = 1; i < core_id + 1; i ++){
        base += bases[i];
    }
    
    for (int i = start_num; i < end_num && i < n_size; i++){
        out_parallel[i] = base + out_parallel[i];
    }
    
    pthread_exit(NULL);
}

void print_vector(vector<int> * out_parallel){
    for (unsigned int i = 0; i < out_parallel->size(); i++){
        cout << (*out_parallel)[i] <<endl;
    }
    return;
}

#endif
