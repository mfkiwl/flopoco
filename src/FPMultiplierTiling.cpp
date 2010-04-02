/*
  Floating Point Multiplier for FloPoCo
 
  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon
  
  Author: Bogdan Pasca

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,  
  CeCILL license, 2008-2010.

 */

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
#include "FPMultiplierTiling.hpp"
#include "FPNumber.hpp"

using namespace std;

namespace flopoco{

	extern vector<Operator*> oplist;

	FPMultiplierTiling::FPMultiplierTiling(Target* target, int wEX, int wFX, int wEY, int wFY, int wER, int wFR, int norm,float ratio, int maxTimeInMinutes) :
		Operator(target), wEX_(wEX), wFX_(wFX), wEY_(wEY), wFY_(wFY), wER_(wER), wFR_(wFR) {

		ostringstream name;
		name << "FPMultiplierTiling_"<<wEX_<<"_"<<wFX_<<"_"<<wEY_<<"_"<<wFY_<<"_"<<wER_<<"_"<<wFR_; 
		setName(name.str());

		if (wFX!=wFY){
			cerr << "karatsuba, equal sizes for inputs pls";
			throw "karatsuba, equal sizes for inputs pls";
		}


		/* set if operator outputs a normalized_ result */
		normalized_ = (norm==0?false:true);
	
		addFPInput ("X", wEX_, wFX_);
		addFPInput ("Y", wEY_, wFY_);
		if(normalized_==true) 
			addFPOutput ("R"   , wER_, wFR    );  
		else{
			wFR_ = 2 + wFX_ + wFY_;
			addOutput  ("ResultExponent"   , wER_    );  
			addOutput ("ResultSignificand", wFR_    );
			addOutput ("ResultException"  , 2      );
			addOutput ("ResultSign"       , 1      ); 
		}

		/* Sing Handling */
		vhdl << tab << declare("sign") << " <= " << "X" << of(wEX_+wFX) << " xor " << "Y" << of(wEY_+wFY) << ";" << endl;

		/* Exponent Handling */
		vhdl << tab << declare("expX", wEX_) << " <= " << "X"<< range(wEX_ + wFX_ -1, wFX_) << ";" << endl; 
		vhdl << tab << declare("expY", wEY_) << " <= " << "Y"<< range(wEY_ + wFY_ -1, wFX_) << ";" << endl; 

		//Add exponents and substract bias
		vhdl << tab << declare("expSumPreSub", wEX_+2) << " <= " << "(\"00\" & expX) + (\"00\" & expY);" << endl; 
		nextCycle(); ///////////////////////////////////////////////////////
		vhdl << tab << declare("bias", wEX_+2) << " <= " << "CONV_STD_LOGIC_VECTOR(" << intpow2(wER-1)-1 << ","<<wEX_+2<<");"<< endl; 
		vhdl << tab << declare("expSum",wEX+2) << " <= " << use("expSumPreSub") << " - " << use("bias") << ";" << endl;   
	
		/* Significand Handling */
		setCycle(0); 
		vhdl << tab << declare("sigX",1 + wFX_) << " <= \"1\" & X" << range(wFX_-1,0) << ";" << endl;
		vhdl << tab << declare("sigY",1 + wFY_) << " <= \"1\" & Y" << range(wFY_-1,0) << ";" << endl;

		intmult_ = new IntTilingMult(target, wFX_+1, wFY_+1, ratio, maxTimeInMinutes);
		oplist.push_back(intmult_);

		inPortMap( intmult_, "X", "sigX");
		inPortMap( intmult_, "Y", "sigY");
		outPortMap(intmult_, "R", "sigProd");
		vhdl << instance(intmult_, "SignificandMultiplication");

		/* Exception Handling */
		setCycle(0);
		vhdl << tab << declare("excSel",4) <<" <= X"<<range(wEX_ + wFX_ +2, wEX_ + wFX_ + 1) << " & " << "Y"<<range(wEY_ + wFY_ +2, wEY_ + wFY_ +1) << ";" << endl;
		
		vhdl << tab << "with " << use("excSel") << " select " << endl;
		vhdl << tab << declare("exc",2) << " <= \"00\" when  \"0000\" | \"0001\" | \"0100\", " << endl;
		vhdl << tab << "       \"01\" when \"0101\","<<endl;
		vhdl << tab << "       \"10\" when \"0110\" | \"1001\" | \"1010\" ,"<<endl;		
		vhdl << tab << "       \"11\" when others;"<<endl;		

		//syncronization
		syncCycleFromSignal("sigProd");
		syncCycleFromSignal("expSum");
		nextCycle();///////////////////////////////	
		
		if (normalized_==true){
		/******************************************************************/
				
			vhdl << tab<< declare("norm") << " <= " << use("sigProd") << of(wFX_+wFY_+1) << ";"<<endl;
			vhdl << tab<< declare("expPostNorm", wEX_+2) << " <= " << use("expSum") << " + (" << zg(wEX_+1,0) << " & " << use("norm") << ");"<<endl;

			//check is rounding is needed
			if (1+wFR_ >= wFX_+wFY_+2) {  /* => no rounding needed - possible padding */
				vhdl << tab << declare("resSig", wFR_) << " <= " << use("sigProd")<<range(wFX_+wFY_,0) << " & " <<   zg(1+wFR_ - (wFX_+wFY_+2) , 0)<<" when "<< use("norm")<< "='1' else"<<endl;
				vhdl << tab <<"                      "           << use("sigProd")<<range(wFX_+wFY_-1,0) << " & " << zg(1+wFR_ - (wFX_+wFY_+2) + 1 , 0) << ";"<<endl;

				vhdl << tab <<"with "<<use("expPostNorm")<< range(wER_+1, wER_) << " select"<<endl;		
				vhdl << tab << declare("excPostNorm",2) << " <=  \"01\" " << " when  \"00\","<<endl;
				vhdl << tab <<"                            \"10\"             when \"01\", "<<endl;
				vhdl << tab <<"                            \"00\"             when \"11\"|\"10\","<<endl;
				vhdl << tab <<"                            \"11\"             when others;"<<endl;						
				
				vhdl << tab << "with " << use("exc") << " select " << endl;
				vhdl << tab << declare("finalExc",2) << " <= " << use("exc") << " when " << " \"11\"|\"10\"|\"00\"," <<endl;
				vhdl << tab << "                    " << use("excPostNorm") << " when others; " << endl;
		
				vhdl << tab << "R <= " << use("finalExc") << " & " << use("sign") << " & " << use("expPostNorm") << range(wER_-1, 0) << " & " <<use("resSig") <<";"<<endl;
			}
			else{
				nextCycle();////
//				vhdl << tab << declare("resSig", wFR_) << " <= " << use("sigProd")<<range(wFX_+wFY_, wFX_+wFY_ - wFR_+1) <<" when "<< use("norm")<< "='1' else"<<endl;
//				vhdl << tab <<"                      "           << use("sigProd")<<range(wFX_+wFY_-1, wFX_+wFY_ - wFR_) << ";"<<endl;

				vhdl << tab << declare("sigProdExt", wFX_+wFY_+ 1 + 1) << " <= " << use("sigProd") << range(wFX_+wFY_, 0) << " & " << zg(1,0) <<" when "<< use("norm")<< "='1' else"<<endl;
				vhdl << tab << "                      "           << use("sigProd") << range(wFX_+wFY_-1, 0) << " & " << zg(2,0) << ";"<<endl;
							
				vhdl << tab << declare("expSig", 2 + wER_ + wFR_) << " <= " << use("expPostNorm") << " & " << use("sigProdExt")<<range(wFX_+wFY_+ 1,wFX_+wFY_+ 2 -wFR_) << ";" << endl;

		
				vhdl << tab << declare("sticky") << " <= " << use("sigProdExt") << of(wFX_+wFY + 1 - wFR) << ";" << endl;
				vhdl << tab << declare("guard") << " <= " << "'0' when " << use("sigProdExt") << range(wFX_+wFY + 1 - wFR - 1,0) << "=" << zg(wFX_+wFY + 1 - wFR - 1 +1,0) <<" else '1';" << endl;
				vhdl << tab << declare("round") << " <= " << use("sticky") << " and ( (" << use("guard") << " and not(" << use("sigProdExt") << of(wFX_+wFY + 1 - wFR+1) <<")) or (" 
				                                                                         << use("sigProdExt") << of(wFX_+wFY + 1 - wFR+1) << " ))  " <<";" << endl;
				                                                                      
				nextCycle();////			
				intadd_ = new IntAdder(target, 2 + wER_ + wFR_);
				oplist.push_back(intadd_);
				
				inPortMap    (intadd_, "X",   "expSig");
				inPortMapCst (intadd_, "Y",   zg(2 + wER_ + wFR_,0));
				inPortMap    (intadd_, "Cin", "round");
				outPortMap   (intadd_, "R", "expSigPostRound");
				
				vhdl << tab << instance( intadd_, "RoundingAdder");
				syncCycleFromSignal("expSigPostRound");
				nextCycle();
				vhdl << tab <<"with "<<use("expSigPostRound")<< range(wER_+wFR_+1, wER_+wFR_) << " select"<<endl;		
				vhdl << tab << declare("excPostNorm",2) << " <=  \"01\" " << " when  \"00\","<<endl;
				vhdl << tab <<"                            \"10\"             when \"01\", "<<endl;
				vhdl << tab <<"                            \"00\"             when \"11\"|\"10\","<<endl;
				vhdl << tab <<"                            \"11\"             when others;"<<endl;						
			
				vhdl << tab << "with " << use("exc") << " select " << endl;
				vhdl << tab << declare("finalExc",2) << " <= " << use("exc") << " when " << " \"11\"|\"10\"|\"00\"," <<endl;
				vhdl << tab << "                    " << use("excPostNorm") << " when others; " << endl;
	
				vhdl << tab << "R <= " << use("finalExc") << " & " << use("sign") << " & " << use("expSigPostRound") << range(wER_+wFR_-1, 0)<<";"<<endl;
			
			}
		}else{ //the non-normalized version for DotProduct
				vhdl << tab <<"with "<<use("expSum")<< range(wER_+1, wER_) << " select"<<endl;		
				vhdl << tab << declare("excPostProc",2) << " <=  \"01\" " << " when  \"00\","<<endl;
				vhdl << tab <<"                            \"10\"             when \"01\", "<<endl;
				vhdl << tab <<"                            \"00\"             when \"11\"|\"10\","<<endl;
				vhdl << tab <<"                            \"11\"             when others;"<<endl;						
				
				vhdl << tab << "with " << use("exc") << " select " << endl;
				vhdl << tab << declare("finalExc",2) << " <= " << use("exc") << " when " << " \"11\"|\"10\"|\"00\"," <<endl;
				vhdl << tab << "                    " << use("excPostProc") << " when others; " << endl;
				
				vhdl << tab << "ResultExponent <= " << use("expSum") << range(wER_-1, 0) << ";" << endl;
				vhdl << tab << "ResultSignificand <= " << use("sigProd") << ";" << endl;
				vhdl << tab << "ResultException <= " << use("finalExc") << ";" << endl;
				vhdl << tab << "ResultSign <= " << use("sign") << ";" << endl;
		}
	} // end constructor

	FPMultiplierTiling::~FPMultiplierTiling() {
	}

	// FIXME: the following is only functional for a correctly rounded multiplier.
	// The old code for the non-normalized case is commented out below, just in case.
	void FPMultiplierTiling::emulate(TestCase * tc)
	{
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");
		mpz_class svY = tc->getInputValue("Y");

		/* Compute correct value */
		FPNumber fpx(wEX_, wFX_), fpy(wEY_, wFY_);
		fpx = svX;
		fpy = svY;
		mpfr_t x, y, r;
		mpfr_init2(x, 1+wFX_);
		mpfr_init2(y, 1+wFY_);
		mpfr_init2(r, 1+wFR_); 
		fpx.getMPFR(x);
		fpy.getMPFR(y);
		mpfr_mul(r, x, y, GMP_RNDN);

		// Set outputs 
		FPNumber  fpr(wER_, wFR_, r);
		mpz_class svR = fpr.getSignalValue();
		tc->addExpectedOutput("R", svR);

		// clean up
		mpfr_clears(x, y, r, NULL);
	}


}