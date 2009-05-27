/*
 * An integer adder for FloPoCo
 *
 * It may be pipelined to arbitrary frequency.
 * Also useful to derive the carry-propagate delays for the subclasses of Target
 *
 * Authors : Florent de Dinechin, Bogdan Pasca
 *
 * This file is part of the FloPoCo project developed by the Arenaire
 * team at Ecole Normale Superieure de Lyon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
*/

#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "../utils.hpp"
#include "../Operator.hpp"
#include "../IntNAdder.hpp"
#include "IntIntKCM.hpp"
#include "FirstKCMTable.hpp"
#include "LastKCMTable.hpp"

using namespace std;
extern vector<Operator *> oplist;

IntIntKCM::IntIntKCM(Target* target, int wIn, mpz_class C, map<string, double> inputDelays):
Operator(target), wIn_(wIn), C_(C), inputDelays_(inputDelays) 
{
	// Set up the IO signals
	addInput ("X" , wIn_);

	//ititialize the output width
	wOut_ = intlog2( (C<0? (0-C) :C) ) + 1 +  wIn_;
	addOutput("R" , wOut_);

	ostringstream name;
	name << "IntIntKCM_" << wIn_<<"_"<< wOut_;
	setName(name.str());


	if (verbose){
		cout <<"delay for X is   "<< inputDelays["X"]<<endl;	
	}

//      compute the maximum input delay
//		maxInputDelay = 0;
//		map<string, double>::iterator iter;
//		for (iter = inputDelays.begin(); iter!=inputDelays.end();++iter)
//			if (iter->second > maxInputDelay)
//				maxInputDelay = iter->second;
//	
//		if (verbose)
//			cout << "The maximum input delay is "<<	maxInputDelay<<endl;
//	
//		double	objectivePeriod;
//		objectivePeriod = 1/ target->frequency();
//		
//		if (verbose)
//			cout << "Objective period is "<< objectivePeriod<<" at an objective frequency of "<<target->frequency() << endl;
//TODO <= do chunk size
	
	int constantWidth = intlog2( (C < 0?0-C:C) ) + 1; //the constant is represented in 2's complement
	int lutWidth = target->lutInputs();
	int nbOfTables = int ( ceil( double(wIn)/double(lutWidth)) );
	int lastLutWidth = (wIn%lutWidth==0?lutWidth: wIn%lutWidth);
	
	FirstKCMTable* t1;
	LastKCMTable* t2;
	
	if (verbose){
		cout << "The width of the constant in 2's complement is = " << constantWidth<<endl;
		cout << "The number of inputs / LUT is = " << lutWidth << endl;
		cout << "The number of tables needed is = " << nbOfTables << endl;
	
	}
	
	if (nbOfTables>1) {
		//let us build one table 
		t1 = new FirstKCMTable(target, lutWidth, constantWidth + lutWidth, C);
		oplist.push_back(t1);
	}
		
	t2 = new LastKCMTable(target, lastLutWidth , constantWidth + lastLutWidth, C);
	oplist.push_back(t2);
		
	
	//the last table will have a variable number of inputs
		//TODO
	
	//first split the input X into digits having lutWidth bits -> this is as generic as it gets :)
		for (int i=0; i<nbOfTables; i++)
			if (i<nbOfTables-1)
				vhdl << tab << declare( join("d",i), lutWidth ) << " <= " << use("X") << range(4*(i+1)-1, 4*i ) << ";" <<endl;
			else
				vhdl << tab << declare( join("d",i), wIn -  4*i ) << " <= " << use("X") << range( wIn-1 , 4*i ) << ";" <<endl;
	
	cout << "Generating the maps on the tables ... "<<endl;
	//perform nbOfTables multiplications
		for ( int i=nbOfTables-1; i >= 0; i--){
			if (i==(nbOfTables-1)){
				//instantiate the KCMLastTable
				inPortMap (t2, "X", join("d",i));
				outPortMap(t2, "Y", join("pp",i));
				vhdl << instance(t2, join("LastKCMTable_SignedMul",i));
				cout << "Generated map for last table " << endl;
			}else{
				cout << "Generated map for first table " << endl;
				//instantiate the KCMLastTable
				inPortMap (t1, "X", join("d",i));
				outPortMap(t1, "Y", join("pp",i));
				vhdl << instance(t1, join("FirstKCMTable_UnsignedMul",i));
			}
		}
		
		//determine the addition operand size
		int addOpSize = (nbOfTables - 2) * lutWidth + (constantWidth + 	lastLutWidth);
		
		for (int i=0; i<nbOfTables; i++){
			vhdl << tab << declare( join("addOp",i), addOpSize ) << " <= ";
				if (i!=nbOfTables-1){
					for (int j=addOpSize-1; j>= (constantWidth + lutWidth) + (i-1)*lutWidth ; j--)
						vhdl << use(join("pp",i))<<range(constantWidth + lutWidth -1,constantWidth + lutWidth -1) << " & ";
					
					vhdl << use(join("pp",i)) << range(constantWidth + lutWidth -1, (i==0?lutWidth:0)) << " & " << zeroGenerator((i-2)*lutWidth,0) << ";" << endl;
				}
				else{
						for (int j=addOpSize-1; j>= (constantWidth + lastLutWidth)+ (i-1)*lutWidth ; j--)
						vhdl << use(join("pp",i))<<range(constantWidth + lastLutWidth -1,constantWidth + lastLutWidth -1) << " & ";
					
					vhdl << use(join("pp",i))<<range(constantWidth + lastLutWidth -1, (i==0?lutWidth:0))<< " & " << zeroGenerator((i-1)*lutWidth,0) << ";" << endl;
				}
		}
		
		IntNAdder* adder = new IntNAdder(target, addOpSize, nbOfTables);
		oplist.push_back(adder);
		for (int i=0; i<nbOfTables; i++)
			inPortMap (adder, join("X",i) , join("addOp",i));
		
		inPortMapCst(adder, "Cin", "'0'");
		outPortMap(adder, "R", "OutRes");
		vhdl << instance(adder, "Result_Adder");
		
		syncCycleFromSignal("OutRes");
		
		vhdl << tab << "R <= " << use("OutRes") << " & " << use( "pp0")<<range(lutWidth-1,0) << ";" <<endl;
		

}

IntIntKCM::~IntIntKCM() {
}


void IntIntKCM::emulate(TestCase* tc)
{
	mpz_class svX =  tc->getInputValue("X");
	cout << "-------------------------"<<endl;
	cout << "X="<<unsignedBinary(svX, wIn_);
	bool xneg = false;
	//x is in 2's complement, so it's value is 
	if ( svX > ( (1<<(wIn_-1))-1) ){
		xneg = true;
		svX = svX - (1<<(wIn_-1));
		svX = (1<<(wIn_-1)) - svX;
	}
		
	mpz_class svR;

	svR = svX * C_;
	
	if (xneg)
		svR = (1<<(wIn_-1)) + svR;
	
 	tc->addExpectedOutput("R", svR);
}

