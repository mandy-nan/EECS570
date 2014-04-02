//
//  main.cpp
//  P1
//
//  Created by Fiona Yang on 1/14/14.
//  Copyright (c) 2014 University of Michigan. All rights reserved.
//

#include <iostream>
#include <vector>
#include <sys/time.h>
#include <pthread.h>
#include "prefix_sum.h"

using namespace std;

int main(int argc, const char * argv[])
{
    int n;
    //n = 1000000;
    //core_num = 2;
    n = atoi(argv[1]);
    core_num = atoi(argv[2]);
    
    vector<int> series(n);
    vector<int> out_serial;
    vector<int> out_parallel;
    struct timeval starttime_serial,endtime_serial;
	struct timeval starttime_parallel,endtime_parallel;
    long long parallel_time,serial_time;
    
    for (int i = 0; i < n; i++){
        series[i] = rand() % 100; // Range from 0 - 100
        //cout<<"x"<<i + 1<<" = "<<series[i]<<endl;
    }
    
	gettimeofday(&starttime_serial,NULL);
    out_serial = serial_prefixSum(series);
	gettimeofday(&endtime_serial,NULL);
    
	gettimeofday(&starttime_parallel,NULL);
    out_parallel = parallel_prefixSum(series, core_num);
	gettimeofday(&endtime_parallel,NULL);
    
    serial_time   = ((endtime_serial.tv_sec * 1000000 + endtime_serial.tv_usec) - (starttime_serial.tv_sec * 1000000 + starttime_serial.tv_usec));
    parallel_time = (((endtime_parallel.tv_sec * 1000000 + endtime_parallel.tv_usec) - (starttime_parallel.tv_sec * 1000000 + starttime_parallel.tv_usec)));
    cout << "Sequential cost: " << serial_time << "ms" << endl;
    cout << "Parallel cost: " << parallel_time << "ms" << endl;
    
    for (int i = 0; i < n; i++){
        assert(out_serial[i] == out_parallel[i]);
    }

    return 0;
}

