#ifndef FunctionEvaluator_HPP
#define FunctionEvaluator_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "../Operator.hpp"
#include "PolyTableGenerator.hpp"
#include "../IntAdder.hpp"

namespace flopoco{

	extern map<string, double> emptyDelayMap;


	/** The FunctionEvaluator class */
	class FunctionEvaluator : public Operator
	{
	public:
		/**
		 * The FunctionEvaluator constructor
		 */
		FunctionEvaluator(Target* target, string func, int wInX, int lsbOut, int n, bool finalRounding = true, map<string, double> inputDelays = emptyDelayMap);

		/**
		 * FunctionEvaluator destructor
		 */
		~FunctionEvaluator();
		
		void emulate(TestCase * tc);

		PiecewiseFunction *pf;
		PolyTableGenerator *tg;
		PolynomialEvaluator *pe;
		
		int getRWidth(){
			return wR;
		}
			
		int getRWeight(){
			return weightR;
		}

		protected:
		
		unsigned wR;
		int weightR;

		int wInX_;   
		int lsbOut_;
		bool finalRounding_;
		
	};

}

#endif