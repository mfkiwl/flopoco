/*
  Polynomial Function Evaluator for FloPoCo
	This version uses piecewise polynomial approximation for a trade-off between tables and multiplier hardware.
	
  Authors: Florent de Dinechin (rewrite from scratch of original code by Mioara Joldes and Bogdan Pasca, see the attic directory)

  This file is part of the FloPoCo project
	launched by the Arénaire/AriC team of Ecole Normale Superieure de Lyon
  currently developed by the Socrate team at CITILab/INSA de Lyon
 
  Initial software.
  Copyright © ENS-Lyon, INSA-Lyon, INRIA, CNRS, UCBL,  
  2008-2014.
  All rights reserved.

  */

#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <string.h>

#include <gmp.h>
#include <mpfr.h>

#include <gmpxx.h>
#include "../utils.hpp"


#include "FixFunctionByPiecewisePoly.hpp"
#include "FixFunctionByTable.hpp"
#include "FixHornerEvaluator.hpp"

using namespace std;

namespace flopoco{

	/* Error analysis:
		 Target error is exp2(lsbOut).
		 Final rounding may entail up to exp2(lsbOut-1).
		 So approximation+rounding error budget is  exp2(lsbOut-1).
		 We call PiecewisePolyApprox with approximation target error bound exp2(lsbOut-2).
		 It reports polyApprox->LSB: may be lsbOut-2, may be up to lsbOut-2- intlog2(degree) 
		 It also reports polyApprox->approxErrorBound
		 so rounding error budget is  exp2(lsbOut-1) - polyApprox->approxErrorBound
		 Staying within this budget is delegated to FixHornerEvaluator: see there for the details
			 
	 */

	
#define DEBUGVHDL 0

	FixFunctionByPiecewisePoly::CoeffTable::CoeffTable(Target* target, int wIn, int wOut, PiecewisePolyApprox* polyApprox_, bool addFinalRoundBit_, int finalRoundBitPos_) :
		Table(target, wIn, wOut), polyApprox(polyApprox_), addFinalRoundBit(addFinalRoundBit_), finalRoundBitPos(finalRoundBitPos_)
	{
		srcFileName="FixFunctionByPiecewisePoly::CoeffTable";
		ostringstream name; 
		name <<"CoeffTable_" << wIn << "_" << wOut << "_" << getNewUId();
		setName(name.str());
		// outDelayMap["Y"] = target->RAMDelay(); // is this useful?
	}


	mpz_class FixFunctionByPiecewisePoly::CoeffTable::function(int x){
		mpz_class z=0;
		int currentShift=0;
		for(int i=polyApprox->degree; i>=0; i--) {
			z += (polyApprox-> getCoeff(x, i)) << currentShift; // coeff of degree i from poly number x
			// REPORT(DEBUG, "i=" << i << "   z=" << unsignedBinary(z, 64));
			if(i==0 && addFinalRoundBit){ // coeff of degree 0
				z += mpz_class(1)<<(currentShift + finalRoundBitPos - polyApprox->LSB); // add the round bit
				//REPORT(DEBUG, "i=" << i << " + z=" << unsignedBinary(z, 64));
				// This may entail an overflow of z, in the case -tiny -> +tiny, e.g. first polynomial of atan
				// This is OK modulo 2^wOut (two's complement), but will break the vhdl output of Table: fix it here.
				z = z & ((mpz_class(1)<<wOut) -1);
				// REPORT(INFO, "Adding final round bit at position " << finalRoundBitPos-polyApprox->LSB);
			}
			currentShift +=  polyApprox->MSB[i] - polyApprox->LSB +1;
		}
		return z;
	}





	FixFunctionByPiecewisePoly::FixFunctionByPiecewisePoly(Target* target, string func, int lsbIn, int msbOut, int lsbOut, int degree_, bool finalRounding_, double approxErrorBudget_, map<string, double> inputDelays):
		Operator(target, inputDelays), degree(degree_), finalRounding(finalRounding_), approxErrorBudget(approxErrorBudget_){

		if(finalRounding==false){
			THROWERROR("FinalRounding=false not implemented yet" );
		}

		f=new FixFunction(func, false, lsbIn, msbOut, lsbOut); // this will provide emulate etc.
		
		srcFileName="FixFunctionByPiecewisePoly";
		
		ostringstream name;
		name<<"FixFunctionByPiecewisePoly_"<<getNewUId(); 
		setName(name.str()); 

		setCriticalPath(getMaxInputDelays(inputDelays));

		setCopyrightString("Florent de Dinechin (2014)");
		addHeaderComment("-- Evaluator for " +  f-> getDescription() + "\n"); 
		REPORT(DETAILED, "Entering: FixFunctionByPiecewisePoly \"" << func << "\" " << lsbIn << " " << msbOut << " " << lsbOut << " " << degree);
 		int wX=-lsbIn;
		addInput("X", wX);
		int outputSize = msbOut-lsbOut+1; // TODO finalRounding would impact this line
		addOutput("Y" ,outputSize , 2);
		useNumericStd();

		if(degree==0){ // This is a simple table
			REPORT(DETAILED, "Degree 0: building a simple table");
			FixFunctionByTable* table=new FixFunctionByTable(target, func, false, lsbIn, msbOut, lsbOut);
			addSubComponent(table);

			inPortMap(table, "X", "X");
			outPortMap(table, "Y", "YR");
			vhdl << instance(table, "simpleTable") << endl;
			syncCycleFromSignal("YR");
			vhdl << tab << "Y <= YR;" << endl; 
		}
		else{
			if(degree==1){ // For degree 1, MultiPartite could work better
			REPORT(INFO, "Degree 1: You should consider using FixFunctionByMultipartiteTable");
			}
			// Build the polynomial approximation
			REPORT(DETAILED, "Computing polynomial approximation for target precision "<< lsbOut-2);
			double targetAcc= approxErrorBudget*pow(2, lsbOut);
			polyApprox = new PiecewisePolyApprox(func, targetAcc, degree);
			int alpha =  polyApprox-> alpha; // coeff table input size 

			// Resize its MSB to the one input by the user. This is useful for functions that "touch" a power of two and may thus overflow by one bit.
			for (int i=0; i<(1<<alpha); i++) {
				// REPORT(DEBUG, "i=" << i << "   coeff 0 before resizing: " <<   polyApprox -> poly[i] -> coeff[0] ->getBitVector());
				polyApprox -> poly[i] -> coeff[0] -> changeMSB(msbOut);
				// REPORT(DEBUG, "i=" << i << "   coeff 0 after resizing: " <<   polyApprox -> poly[i] -> coeff[0] ->getBitVector());
			}
			polyApprox -> MSB[0] = msbOut;

			// Store it in a table
			int polyTableOutputSize=0;
			for (int i=0; i<=degree; i++) {
				polyTableOutputSize += polyApprox->MSB[i] - polyApprox->LSB +1;
			} 
			REPORT(DETAILED, "Poly table input size  = " << alpha);
			REPORT(DETAILED, "Poly table output size = " << polyTableOutputSize);

			double roundingErrorBudget=exp2(lsbOut-1)-polyApprox->approxErrorBound;
			REPORT(DETAILED, "Overall error budget = " << exp2(lsbOut) << "  of which approximation error = " << polyApprox->approxErrorBound
						 << "   hence rounding error budget = "<< roundingErrorBudget );
		 

			
			// This is where we add the final rounding bit
			FixFunctionByPiecewisePoly::CoeffTable* coeffTable = new CoeffTable(target, alpha, polyTableOutputSize, polyApprox,
																					finalRounding, lsbOut-1 /*position of the round bit*/) ;
			addSubComponent(coeffTable);

			vhdl << tab << declare("A", alpha)  << " <= X" << range(wX-1, wX-alpha) << ";" << endl;
			vhdl << tab << declare("Z", wX-alpha)  << " <= X" << range(wX-alpha-1, 0) << ";" << endl;
			vhdl << tab << declare("Zs", wX-alpha)  << " <= (not Z(" << wX-alpha-1 << ")) & Z" << range(wX-alpha-2, 0) << "; -- centering the interval" << endl;

			inPortMap(coeffTable, "X", "A");
			outPortMap(coeffTable, "Y", "Coeffs");
			vhdl << instance(coeffTable, "coeffTable") << endl;

			syncCycleFromSignal("Coeffs");

			int currentShift=0;
			for(int i=polyApprox->degree; i>=0; i--) {
				vhdl << tab << declare(join("A",i), polyApprox->MSB[i] - polyApprox->LSB +1)
						<< " <= Coeffs" << range(currentShift + (polyApprox->MSB[i] - polyApprox->LSB), currentShift) << ";" << endl;
				currentShift +=  polyApprox->MSB[i] - polyApprox->LSB +1;
			}

			// Here I wish I could plug more parallel evaluators. Hence the interface.
			
			// This builds an architecture such as eps_finalround < 2^(lsbOut-1) and eps_round<2^(lsbOut-2)
			FixHornerEvaluator* horner = new FixHornerEvaluator(target, lsbIn+alpha+1, msbOut, lsbOut, degree, polyApprox->MSB, polyApprox->LSB, roundingErrorBudget);		
			addSubComponent(horner);

			inPortMap(horner, "X", "Zs");
			outPortMap(horner, "R", "Ys");
			for(int i=0; i<=polyApprox->degree; i++) {
				inPortMap(horner,  join("A",i),  join("A",i));
			}
			vhdl << instance(horner, "horner") << endl;
			syncCycleFromSignal("Ys");
		
			vhdl << tab << "Y <= " << "std_logic_vector(Ys);" << endl;
		}
	}



	FixFunctionByPiecewisePoly::~FixFunctionByPiecewisePoly() {
		free(f);
	}
	


	void FixFunctionByPiecewisePoly::emulate(TestCase* tc){
		f->emulate(tc);
	}

	void FixFunctionByPiecewisePoly::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;

		tc = new TestCase(this); 
		tc->addInput("X", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addInput("X", (mpz_class(1) << f->wIn) -1);
		emulate(tc);
		tcl->add(tc);

	}



	
	OperatorPtr FixFunctionByPiecewisePoly::parseArguments(Target *target, vector<string> &args) {
		int lsbIn, msbOut, lsbOut, d;
		string f;
		double approxErrorBudget;
		UserInterface::parseString(args, "f", &f); 
		UserInterface::parseInt(args, "lsbIn", &lsbIn);
		UserInterface::parseInt(args, "msbOut", &msbOut);
		UserInterface::parseInt(args, "lsbOut", &lsbOut);
		UserInterface::parsePositiveInt(args, "d", &d);
		UserInterface::parseFloat(args, "approxErrorBudget", &approxErrorBudget);
		return new FixFunctionByPiecewisePoly(target, f, lsbIn, msbOut, lsbOut, d, true, approxErrorBudget);
	}

	void FixFunctionByPiecewisePoly::registerFactory(){
		UserInterface::add("FixFunctionByPiecewisePoly", // name
											 "Evaluator of function f on [0,1), using a piecewise polynomial of degree d with Horner scheme.",
											 UserInterface::FunctionApproximation,
											 "",
											 "f(string): function to be evaluated between double-quotes, for instance \"exp(x*x)\";\
                        lsbIn(int): weight of input LSB, for instance -8 for an 8-bit input;\
                        msbOut(int): weight of output MSB;\
                        lsbOut(int): weight of output LSB;\
                        d(int): degree of the polynomial;\
                        approxErrorBudget(float)=0.25: error budget in ulp for the approximation.",                        
											 "This operator uses a table for coefficients, and Horner evaluation with truncated multipliers sized just right.<br>For more details, see <a href=\"bib/flopoco.html#DinJolPas2010-poly\">this article</a>.",
											 FixFunctionByPiecewisePoly::parseArguments
											 ) ;
		
	}

}
	
