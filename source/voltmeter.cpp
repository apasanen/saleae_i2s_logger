#include <cstdlib>
#include <iostream>
#include <cstring>
#include <limits>
#include <math.h>
#include "voltmeter.hpp"

VoltMeter::VoltMeter(int channels, int threshold, double min, double max,
		     int nsteps, bool enable_graphics)
:channels_(channels), min_(min), max_(max), nsteps_(nsteps), 
    enable_graphics_(enable_graphics), max_value_counter_threshold_(threshold)
{
	max_values_ = new double [channels_];
	max_value_counter_ = new int [channels_];
	marker_ = new char [nsteps_+1];
	marker_[nsteps_] = '\0';

	for (int i=0;i<channels_;i++){
		max_value_counter_[i] = max_value_counter_threshold_;
		max_values_[i] = min_;
		//    draw(max_values_[i], max_values_[i]);
	}
}

VoltMeter::~VoltMeter()
{
	delete[] max_values_;
	delete[] max_value_counter_;
	delete[] marker_;
}

int VoltMeter::bin(double value)
{
	double delta = (max_ - min_) / nsteps_;
	double threshold = min_;
	for (int i=0;i<nsteps_;i++){
		threshold += delta;
		if (value < threshold){
			return i;
		}
	}
	return nsteps_;
}

void VoltMeter::draw(int channel, double value, double max_value)
{
	int tmp = bin(value);
	for (int i=0;i<nsteps_;i++){
		marker_[i] = ' ';
	}
	for (int i=0;i<tmp;i++){
		marker_[i] = '#';
	}
	marker_[bin(max_value)] = 'X';
	printf("%d.", channel);
	printf("|%s| %.2f                                                    \n", marker_, value);
}


void VoltMeter::set(double values[])
{
	for (int i=0;i<channels_;i++){
		if(enable_graphics_){
			max_value_counter_[i]++;
			if(max_value_counter_[i] > max_value_counter_threshold_){
				max_values_[i] = min_;
			}
			if (max_values_[i] < values[i]){
				max_values_[i] = values[i];
				max_value_counter_[i] = 0;
			}
			draw(i, values[i], max_values_[i]);
		} else {
			printf("%.2f ", values[i]);
		}
	}
}
