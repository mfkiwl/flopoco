/* vim: set tabstop=8 softtabstop=2 shiftwidth=2: */
#ifndef __BIGTESTBENCH_HPP
#define __BIGTESTBENCH_HPP

#include "Operator.hpp"

/**
 * Creates a BigTestBench, which tests a certain Operator.
 * The test cases are generated by the unit under test (UUT).
 */
class BigTestBench : public Operator
{
public:
	/* XXX: To discuss what n represents. */
	/**
	 * Creates a BigTestBench.
	 * @param target The target architecture
	 * @param op The operator which is the UUT
	 * @param n Number of tests
	 */
	BigTestBench(Target *target, Operator *op, int n);
	~BigTestBench();

	void output_vhdl(ostream& o, string name);
private:
	/** The unit under test UUT */
	Operator *op;

	/** The parameter from the constructor */
	int n;
};


#endif

