/*
  Floating Point Square Root, polynomial version, for FloPoCo
 
  Authors : Mioara Joldes, Bogdan Pasca (2010)

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon
  
  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,  
  CeCILL license, 2008-2010.
*/

#ifdef HAVE_SOLLYA
 
#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <string.h>

#include <gmp.h>
#include <mpfr.h>

#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"
#include "IntAdder.hpp"
#include "IntMultiplier.hpp"
#include "IntSquarer.hpp"
#include "FPSqrtPoly.hpp"

using namespace std;

namespace flopoco{
	extern vector<Operator*> oplist;

	FPSqrtPoly::FPSqrtPoly(Target* target, int wE, int wF, bool correctlyRounded, int degree):
		Operator(target), wE(wE), wF(wF), correctRounding(correctlyRounded) {

		ostringstream name;
		name<<"FPSqrtPoly_"<<wE<<"_"<<wF;
		uniqueName_ = name.str(); 

		// -------- Parameter set up -----------------
		addFPInput ("X", wE, wF);
		addFPOutput("R", wE, wF);


		vhdl << tab << declare("excsX",3) << " <= X"<<range(wE+wF+2,wE+wF)<<";"<<endl;
		vhdl << tab << declare("sX",1) << "  <= X"<<of(wE+wF)<<";"<<endl;
		vhdl << tab << declare("expX",wE) << " <= X"<<range(wE+wF-1,wF)<<";"<<endl;
		vhdl << tab << declare("fX",wF+1) << " <= \"1\" & X"<<range(wF-1,0 )<<";"<<endl;
		
		vhdl << "--If the real exponent is odd"<<endl;
		vhdl << tab << declare("OddExp")    << " <= not(expX(0));"  << endl;  

		//first estimation of the exponent
		vhdl << tab << declare("expBiasPostDecrement", wE+1) << " <= " << "CONV_STD_LOGIC_VECTOR("<< (1<<(wE-1))-2 <<","<<wE+1<<")"<<";"<<endl;
		vhdl << tab << declare("expPostBiasAddition", wE+1) << " <= " << "( \"0\" & expX) + expBiasPostDecrement + not(OddExp);"<<endl;
	
		vhdl << tab << "-- sign/exception handling" << endl;
		vhdl << tab << "with excsX select" <<endl
			  << tab << tab <<  declare("exnR", 2) << " <= " << "\"01\" when \"010\", -- positive, normal number" << endl
			  << tab << tab << "excsX" << range(2, 1) << " when \"001\" | \"000\" | \"100\", " << endl
			  << tab << tab << "\"11\" when others;"  << endl;

//		FunctionEvaluator *fixpsqrt = new FunctionEvaluator(target, "sqrt(2+2*x),0,1,1;sqrt(1+x),0,1,1", wF+1, wF, degree); //TODO
//		oplist.push_back(fixpsqrt);

//***************************************************************************

		TableGenerator *tg = new TableGenerator(target, "sqrt(2+2*x),0,1,1;sqrt(1+x),0,1,1", wF+1, wF+1, degree);
		oplist.push_back(tg);
		combinatorialOperator = false;
		
		int k1, k2;
		k1 = (tg->getNrIntArray())[0];
		k2 = (tg->getNrIntArray())[1];
		
		int aa = tg->wIn;
	
		int dk1k2 = k1-k2;
		
		vhdl << tab << declare("theAddr",aa) << " <= (expX(0) & X"<<range(wF-1,wF-k1)<<") when expX(0)='0' else "<<endl
		                                 << "(expX(0) & " << zg(dk1k2,0) << " & X"<<range(wF-1,wF-k2)<<");"<<endl;
		vhdl << tab << declare("theFrac",wF-k2) << " <= ("<< zg(dk1k2,0) <<" & X"<<range(wF-k1-1,0)<<") when expX(0)='0' else "<<endl
		                                 << "X"<<range(wF-k2-1,0)<<";"<<endl;
		
		REPORT(DEBUG, "k1="<<k1<<" k2="<<k2);
		
		YVar* y = new YVar(wF-k2, -k2);
		
		PolynomialEvaluator *pe = new PolynomialEvaluator(target, tg->getCoeffParamVector(), y, wF+1, tg->getMaxApproxError() );
		oplist.push_back(pe);
		
//		wR = pe->getRWidth();
//		weightR = pe->getRWeight();

//		vhdl << tab << declare("addr", tg->wIn) << " <= Xaddr;"<<endl;
		nextCycle();/////////////////////////////////// The Coefficent ROM has a registered iunput
		
		inPortMap ( tg, "X", "theAddr");
		outPortMap ( tg, "Y", "Coef");
		vhdl << instance ( tg, "GeneratedTable" );
		
		syncCycleFromSignal("Coef");
		nextCycle();/////////////////////////////////// The Coefficent ROM has a registered output
		
		vhdl << tab << declare ("y",y->getSize()) << " <= theFrac;" << endl;
		
		/* get the coefficients */
		int lsb = 0, sizeS = 0;
		for (uint32_t i=0; i< pe->getCoeffParamVector().size(); i++){
			lsb += sizeS;
			sizeS = pe->getCoeffParamVector()[i]->getSize()+1;
			vhdl << tab << declare(join("a",i), sizeS ) << "<= Coef"<< range (lsb+sizeS-1, lsb) << ";" << endl;
		}
		
		inPortMap( pe, "Y", "y");
		for (uint32_t i=0; i< pe->getCoeffParamVector().size(); i++){
			inPortMap(pe,  join("a",i), join("a",i));
		}
		outPortMap( pe, "R", "rfx");
		vhdl << instance( pe, "PolynomialEvaluator");
		
		syncCycleFromSignal("rfx");


//***************************************************************************


//		inPortMap(fixpsqrt, "Xaddr", "theAddr");
//		inPortMap(fixpsqrt, "Xfrac", "theFrac");
//		outPortMap(fixpsqrt, "R", "rfx");
//		vhdl << instance(fixpsqrt, "FixPointSQRT");
//		
//		syncCycleFromSignal("rfx");
		
		
//		vhdl << tab << declare("sticky",1) << " <=  rfx"<<of(pe->getRWidth()-(pe->getRWeight()+wF)-1)<<";"<<endl;
		
		
		vhdl << tab << declare("sticky",1) << " <= '0' when rfx"<<range(pe->getRWidth()-(pe->getRWeight()+wF)-2,0) <<" = " 
		                                                        << zg(pe->getRWidth()-(pe->getRWeight()+wF)-1,0) << " else '1';"<<endl;
		
		vhdl << tab << declare("extentedf", 1 + wF + 2) << " <= rfx"<<range(pe->getRWidth()-pe->getRWeight(), pe->getRWidth()-(pe->getRWeight()+wF)-1) 
		                                                << " & sticky;"<<endl;
		                  
		nextCycle();                              
		IntAdder *a = new IntAdder(target, 1 + wF + 2);
		oplist.push_back(a);
		
		inPortMap(a, "X", "extentedf");
		inPortMapCst(a, "Y", zg(1 + wF + 2,0) );
		inPortMapCst(a, "Cin", "'1'");
		outPortMap(a, "R", "fPostRound");
		vhdl << instance(a, "Rounding_Adder");

		syncCycleFromSignal("fPostRound");
		

		addOutput("RFull", pe->getRWidth());
		vhdl << tab << " RFull <= rfx;" << endl; 
		
		vhdl << tab << " R <= exnR & \"0\" & expPostBiasAddition"<<range(wE,1)<<" & fPostRound"<<range(wF, 1)<<";"<<endl;
		
		
//		cout << "The result number of bits is " << 	pe->getRWidth();
//		cout << "The result weight is " << 	        pe->getRWeight();
			
		
	}



	FPSqrtPoly::~FPSqrtPoly() {
	}

	void FPSqrtPoly::emulate(TestCase * tc)
	{
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");

		/* Compute correct value */
		FPNumber fpx(wE, wF);
		fpx = svX;
		mpfr_t x, r;
		mpfr_init2(x, 1+wF);
		mpfr_init2(r, 1+wF); 
		fpx.getMPFR(x);

		if(correctRounding) {
			mpfr_sqrt(r, x, GMP_RNDN);
			FPNumber  fpr(wE, wF, r);
			/* Set outputs */
			mpz_class svr= fpr.getSignalValue();
			tc->addExpectedOutput("R", svr);
		}
		else { // faithful rounding 
			mpfr_sqrt(r, x, GMP_RNDU);
			FPNumber  fpru(wE, wF, r);
			mpz_class svru = fpru.getSignalValue();
			tc->addExpectedOutput("R", svru);

			mpfr_sqrt(r, x, GMP_RNDD);
			FPNumber  fprd(wE, wF, r);
			mpz_class svrd = fprd.getSignalValue();
			/* Set outputs */
			tc->addExpectedOutput("R", svrd);
		}

		mpfr_clears(x, r, NULL);
	}




	// One test out of 4 fully random (tests NaNs etc)
	// All the remaining ones test positive numbers.

	void FPSqrtPoly::buildRandomTestCases(TestCaseList* tcl, int n){

		TestCase *tc;
		mpz_class a;

		for (int i = 0; i < n; i++) {
			tc = new TestCase(this); 
			/* Fill inputs */
			if ((i & 3) == 0)
				a = getLargeRandom(wE+wF+3);
			else
				a  = getLargeRandom(wE+wF) + (mpz_class(1)<<(wE+wF+1)); // 010xxxxxx
			tc->addInput("X", a);

			/* Get correct outputs */
			emulate(tc);

			tcl->add(tc);
		}
	}
}

#endif //HAVE_SOLLYA
