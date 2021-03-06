/*
   A divider by a floating-point constant for FloPoCo

  This file is part of the FloPoCo project developed by the Arenaire
  team at Ecole Normale Superieure de Lyon
  
  Author : Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,  
  2008-2011.
  All rights reserved.
*/

 


#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"
#include "FPConstDiv.hpp"
#include "TestBenches/FPNumber.hpp"

using namespace std;


namespace flopoco{


	// The expert version 

	FPConstDiv::FPConstDiv(Target* target, int wEIn_, int wFIn_, int wEOut_, int wFOut_, int d_, int dExp_, int alpha_):
		Operator(target), 
		wEIn(wEIn_), wFIn(wFIn_), wEOut(wEOut_), wFOut(wFOut_), d(d_), dExp(dExp_), alpha(alpha_)
	{
		if(wEOut==0)
			wEOut=wEIn;
		if(wFOut==0)
			wFOut=wFIn;
		srcFileName="FPConstDiv";
		ostringstream name;
		name <<"FPConstDiv_"<<wEIn<<"_"<<wFIn<<"_"<<wEOut<<"_"<<wFOut<<"_"<<d<<"_";
		if(dExp>=0)
			name<<dExp<<"_";
		else
			name<<"M"<<-dExp<<"_";
		if(target->isPipelined()) 
			name << target->frequencyMHz() ;
		else
			name << "comb";
		uniqueName_ = name.str();
		
		uniqueName_=name.str();

		if(wEIn<3 || wEOut <3){
			ostringstream error;
			error << srcFileName << " (" << uniqueName_ << "): exponent size must be at least 3" <<endl;
			throw error.str();
		}

		if(d==0) {
			ostringstream o;
			o << srcFileName << " (" << uniqueName_ << "): ERROR in FPConstDiv, division by 0";
			throw o.str();
		}

			// Constant normalization
		while ((d % 2) ==0) {
				REPORT(DEBUG, "d is even, normalising");
				d = d >>1;
				dExp+=1;
		}
		mantissaIsOne = false;
		if(d==1) {
			REPORT(INFO, "Mantissa of d is 1, dividing by it will be easy"); 
			mantissaIsOne = true;
		}
		
		// Computing the binary64 value of the actual constant, taking exponent into account
		dd = d*intpow2(dExp);

		REPORT(INFO, "dividing by the constant " << dd);



		// Set up the IO signals
		addFPInput("X", wEIn, wFIn);
		addFPOutput("R", wEOut, wFOut);

		setCopyrightString("Florent de Dinechin (2007-2011)");

		int gamma = intlog2(d);
		int s = gamma-1;
		int h = d>>1; 
		int intDivSize = wFIn+1 + s+1;

		vhdl << tab << declare("x_exn",2) << " <=  X("<<wEIn<<"+"<<wFIn<<"+2 downto "<<wEIn<<"+"<<wFIn<<"+1);"<<endl;
		vhdl << tab << declare("x_sgn") << " <=  X("<<wEIn<<"+"<<wFIn<<");"<<endl;
		vhdl << tab << declare("x_exp", wEIn) << " <=  X("<<wEIn<<"+"<<wFIn<<"-1 downto "<<wFIn<<");"<<endl;
		vhdl << tab << declare("x_sig", wFIn+1) << " <= '1' & X("<<wFIn-1 <<" downto 0);"<<endl;

		manageCriticalPath(target->localWireDelay() + target->adderDelay(gamma+1));
		vhdl << tab << declare("Diffmd", gamma+1) << " <=  ('0' & x_sig" << range(wFIn, wFIn-gamma+1)<< ") - ('0' & CONV_STD_LOGIC_VECTOR(" << d << ", " << gamma <<")) ;" << endl;
		vhdl << tab << declare("mltd") << " <=   Diffmd("<< gamma<<");" << endl;

		int mltdCycle=getCurrentCycle();
		double mltdCP=getCriticalPath();

		if(d*intpow2(dExp) >1.0) { // only underflow possible
			vhdl <<endl << tab << "-- exponent processing. For this d we may only have underflow" << endl;
			
			manageCriticalPath(target->localWireDelay() + target->adderDelay(wEOut+1));
			vhdl << tab << declare("r_exp0", wEOut+1) << " <=  ('0' & x_exp) - ( CONV_STD_LOGIC_VECTOR(" << s+1+dExp << ", " << wEOut+1 <<")) + (not mltd);" << endl;
			
			vhdl << tab << declare("underflow") << " <=  r_exp0(" << wEOut << ");" << endl;
			vhdl << tab << declare("r_exp", wEOut) << " <=  r_exp0" << range(wEOut-1, 0) << ";" << endl;
			
			vhdl <<endl << tab << "-- exception flag processing"<<endl; 
			vhdl << tab << declare("r_exn", 2) << " <=  \"00\" when  x_exn=\"01\" and underflow='1' else x_exn" << ";" << endl;
		}
		else
			{
			vhdl <<endl << tab << "-- exponent processing. For this d we may only have overflow" << endl;
			
			manageCriticalPath(target->localWireDelay() + target->adderDelay(wEOut+1));
			vhdl << tab << declare("r_exp0", wEOut+1) << " <=  ('0' & x_exp) + ( CONV_STD_LOGIC_VECTOR(" << -(s+1+dExp) << ", " << wEOut+1 <<")) + (not mltd);" << endl;
			
			vhdl << tab << declare("overflow") << " <=  r_exp0(" << wEOut << ");" << endl;
			vhdl << tab << declare("r_exp", wEOut) << " <=  r_exp0" << range(wEOut-1, 0) << ";" << endl;
			
			vhdl <<endl << tab << "-- exception flag processing"<<endl; 
			vhdl << tab << declare("r_exn", 2) << " <=  \"10\" when  x_exn=\"01\" and overflow='1' else x_exn" << ";" << endl;
			}

		// Back to where we were after the computation of mldt
		setCycle(mltdCycle); 
		setCriticalPath(mltdCP);
		vhdl <<endl << tab << "-- significand processing"<<endl;
		if(mantissaIsOne) {
			vhdl << tab << declare("r_frac", wFOut) << " <= x_sig;"<<endl;
		}
		else {// Actual division
			// mux = diffusion of the control signal + 1 LUT
			manageCriticalPath(target->localWireDelay(wFIn) + target->lutDelay());
			vhdl << tab << declare("divIn0", intDivSize) << " <= '0' & x_sig & CONV_STD_LOGIC_VECTOR(" << h << ", " << s <<");" << endl;
			vhdl << tab << declare("divIn1", intDivSize) << " <= x_sig & '0' & CONV_STD_LOGIC_VECTOR(" << h << ", " << s <<");" << endl;
			vhdl << tab << declare("divIn", intDivSize) << " <= divIn1 when mltd='1' else divIn0;" << endl;
			
			icd = new IntConstDiv(target, intDivSize, d,   alpha, false, inDelayMap("X",target->localWireDelay()+getCriticalPath()));
			addSubComponent(icd);
			
			inPortMap  (icd, "X", "divIn");
			outPortMap (icd, "Q","quotient");
			outPortMap (icd, "R","remainder");
			vhdl << instance(icd, "sig_div");
			
			setCycleFromSignal("remainder"); 
			setCriticalPath(icd->getOutputDelay("R"));
			
			vhdl << tab << declare("r_frac", wFOut) << " <= quotient" << range(wFOut-1, 0) << ";"<<endl;
			
		}
			vhdl << tab << "R <=  r_exn & x_sgn & r_exp & r_frac" << ";" << endl;
	}




	FPConstDiv::~FPConstDiv() {
		// TODO but who cares really
		//		free(icd); TODO but what's the syntax?
	}





	void FPConstDiv::emulate(TestCase *tc)
	{
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");

		/* Compute correct value */
		FPNumber fpx(wEIn, wFIn);
		fpx = svX;
		mpfr_t x, mpd, r;
		mpfr_init2(x, 1+wFIn);
		mpfr_init2(r, 1+wFOut); 
		mpfr_init(mpd); // Should be enough for everybody 
		mpfr_set_d(mpd, dd, GMP_RNDN);
		fpx.getMPFR(x);
		mpfr_div(r, x, mpd, GMP_RNDN);		
		// Set outputs 
		FPNumber  fpr(wEOut, wFOut, r);
		mpz_class svRN = fpr.getSignalValue();
		tc->addExpectedOutput("R", svRN);
		// clean up
		mpfr_clears(x, r, mpd, NULL);
		}


	void FPConstDiv::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;
		mpz_class x;
	
		tc = new TestCase(this); 
		tc->addFPInput("X", 1.0);
		tc->addComment("1.0");
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addFPInput("X", 3.0);
		tc->addComment("3.0");
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addFPInput("X", 9.0);
		tc->addComment("9.0");
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addFPInput("X", 27.0);
		tc->addComment("27.0");
		emulate(tc);
		tcl->add(tc);
	}

	/**
	* Method returning a vector containing values of the valid TestState ts up-to-dated
	* it also update the multimap testMemory and increase the counter for the treated operator
	**/
	void FPConstDiv::nextTest ( TestState * ts ){
		string opName = "FPConstDiv";

		// establishment of the different values, pay attention to the order !!
		do{
			// pick a random num following a specific distribution
			int wE;
			do{
				float wEf  = pickRandomNum ( );
				wE = ceil ( wEf );
			} while ( wE < 3 );
			// modify the number pointed in the involved TestState
			ts -> vectInt [ 0 ] = wE;

			int wF;
			do{
				float wFf = pickRandomNum ( 3.0 * wE );
				wF = ceil ( wFf );
			}while ( wF <= wE );
			ts -> vectInt [ 1 ] = wF;

			int d;
			do{
				float df = pickRandomNum ( 0.0, 5, 3 );
				d = ceil ( df );
			}while ( d > 10 );
			ts -> vectInt [ 2 ] = d;


		}while ( Operator::checkExistence ( *ts, opName ) );
		// Add the operator to testMemory (used by checkExistence for verification)
		Operator::testMemory.insert ( pair < string, TestState > ( opName, *ts) );
		// increase the counter of the treated operator indicating how many tests have been done on it
		ts -> counter++;
	}


	OperatorPtr FPConstDiv::parseArguments(Target *target, vector<string> &args) {
		int wE,wF, d, dExp, alpha;
		UserInterface::parseStrictlyPositiveInt(args, "wE", &wE); 
		UserInterface::parseStrictlyPositiveInt(args, "wF", &wF);
		UserInterface::parseStrictlyPositiveInt(args, "d", &d);
		UserInterface::parseInt(args, "dExp", &dExp);
		UserInterface::parseInt(args, "alpha", &alpha);
		return new FPConstDiv(target, wE, wF,  wE,  wF, d,  dExp, alpha);
	}

	void FPConstDiv::registerFactory(){
		UserInterface::add("FPConstDiv", // name
											 "Correctly rounded floating-point divider by a small constant.",
											 "ConstMultDiv",
											 "", // seeAlso
											 "wE(int): exponent size in bits; \
                        wF(int): mantissa size in bits;  \
                        d(int): small integer to divide by;  \
                        dExp(int)=0: binary exponent of d (the operator will divide by d.2^dExp);  \
                        alpha(int)=-1: Algorithm uses radix 2^alpha. -1 choses a sensible default.",
											 "Correct rounding to the nearest (if you want other rounding modes contact us). This operator is described in <a href=\"bib/flopoco.html#dedinechin:2012:ensl-00642145:1\">this article</a>.",
											 FPConstDiv::parseArguments
											 ) ;
		
		/* Cut because it doesn't simulate properly
                        wEOut(int)=0: output exponent size in bits. If 0, will be equal to wE.; \
                        wFOut(int)=0: output mantissa size in bits. If 0, will be equal to wF.; \
		*/

	}


}

