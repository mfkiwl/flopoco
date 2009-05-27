#ifndef LastKCMTable_HPP
#define LastKCMTable_HPP

#include "../Table.hpp"

class LastKCMTable : public Table
{
public:
	LastKCMTable(Target* target, int wIn, int wOut, mpz_class C);
	
	~LastKCMTable();


	mpz_class function(int x);

	mpz_class C_; //the constant

	int wOut_; /**< the width of the output */
	int wIn_;  /**< the width of the input */
	
};

#endif //LastKCMTable_HPP
