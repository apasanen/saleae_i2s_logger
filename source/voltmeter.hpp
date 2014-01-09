#ifndef VOLTMETER_HPP_
#define VOLTMETER_HPP_

#include <string>
#include <cstdio>

using namespace std;

class VoltMeter
{
public:
	VoltMeter(int channels, int threshold, double min, double max,
		  int nsteps);
	~VoltMeter();

	void set(double tbl[]);

private:
	void draw(double v, double m);
	int bin(double v);
	int channels_;
	double min_;
	double max_;
	int nsteps_;
	int max_value_counter_threshold_;
	double * max_values_;
	int * max_value_counter_;
	char * marker_;
};
#endif
