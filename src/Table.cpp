/*
  A generic class for tables of values

  Author : Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010.
  All rights reserved.

 */


#include <iostream>
#include <sstream>
#include <cstdlib>
#include "utils.hpp"
#include "Table.hpp"

using namespace std;


namespace flopoco{


	int Table::double2input(double x){
		throw string("Error, double2input is being used and has not been overriden");
	}

	double Table::input2double(int x) {
		throw string("Error, input2double is being used and has not been overriden");
	}

	mpz_class Table::double2output(double x){
		throw string("Error, double2output is being used and has not been overriden");
	}

	double Table::output2double(mpz_class x) {
		throw string("Error, output2double is being used and has not been overriden");
	}

#if 0 // TODO some day
	mpz_class Table::mpfr2output(mpfr_t x){
		throw string("Error, mpfr2output is being used and has not been overriden");
	}

	void Table::output2mpfr(mpz_class x, mpfr_t y) {
		throw string("Error, output2mpfr is being used and has not been overriden");
	}
#endif



	Table::Table(Target* target_, int _wIn, int _wOut, int _minIn, int _maxIn, int _logicTable, map<string, double> inputDelays) :
		Operator(target_),
		wIn(_wIn), wOut(_wOut), minIn(_minIn), maxIn(_maxIn)
	{
		srcFileName = "Table";
		if(wIn<0){
			THROWERROR("wIn="<<wIn<<"; Input size cannot be negative"<<endl);
		}
		if(wOut<0){
			THROWERROR("wOut="<<wOut<<"; Output size cannot be negative"<<endl);
		}
		setCopyrightString("Florent de Dinechin (2007-2012)");

		// Set up the IO signals
		addInput ("X"  , wIn, true);
		addOutput ("Y"  , wOut, 1, true);

		if(maxIn==-1) maxIn=(1<<wIn)-1;
		if(minIn<0) {
			cerr<<"ERROR in Table::Table, minIn<0\n";
			exit(EXIT_FAILURE);
		}
		if(maxIn>=(1<<wIn)) {
			cerr<<"ERROR in Table::Table, maxIn too large\n";
			exit(EXIT_FAILURE);
		}
		if((minIn==0) && (maxIn==(1<<wIn)-1))
			full=true;
		else
			full=false;
		if (wIn > 10)
		  REPORT(0, "WARNING: FloPoCo is building a table with " << wIn << " input bits, it will be large.");


		if(_logicTable==1)
			logicTable=true;
		else if (_logicTable==-1)
			logicTable=false;
		else { // the constructor should decide
			logicTable = (wIn <= getTarget()->lutInputs())  ||  (wOut * (mpz_class(1) << wIn) < 0.5*getTarget()->sizeOfMemoryBlock());
			if(!logicTable)
				REPORT(DETAILED, "This table will be implemented in memory blocks");
		}


		// Pipelining is managed as follows:
		// Declaration of the signal TableOut at cycle 0. It will be assigned in outputVHDL() below
		// computation of the needed number of cycles (out of Target etc)
		// The delaying of TableOut will be managed by buildVHDLRegisters() as soon as we have manually defined its lifeSpan

		declare("TableOut",  wOut);
		setCriticalPath(getMaxInputDelays(inputDelays));

		if (logicTable)  {
			// Delay is that of broadcasting the input bits to wOut LUTs, plus the LUT delay itself
			if(wIn <= getTarget()->lutInputs())
				addToCriticalPath(getTarget()->localWireDelay(wOut) + getTarget()->lutDelay());
			else{
				int lutsPerBit=1<<(wIn-getTarget()->lutInputs());
				REPORT(DETAILED, "Building a logic table that uses " << lutsPerBit << " LUTs per output bit");
				// TODO this doesn't take into account the F5 muxes etc: there should be a logicTableDelay() in Target
				// The following is enough for practical sizes, but it is an overestimation.
				addToCriticalPath(getTarget()->localWireDelay(wOut*lutsPerBit) + getTarget()->lutDelay() + getTarget()->localWireDelay() + getTarget()->lutDelay());
			}
		}
		else{
			manageCriticalPath(getTarget()->LogicToRAMWireDelay() + getTarget()->RAMToLogicWireDelay() + getTarget()->RAMDelay()); // will hopefully insert the extra register when needed
		}

		getSignalByName("TableOut") -> updateLifeSpan(getCurrentCycle());
		outDelayMap["Y"] =   getCriticalPath();
	}




	Table::Table(Target* target) :
		Operator(target){
		setCopyrightString("Florent de Dinechin, Bogdan Pasca (2007, 2010)");
	}


	// We have to define this method because the constructor of Table cannot use the (pure virtual) function()
	// If the table has internal pipeline registers, they must be managed manually here and it is a pain

	//this version is currently disabled because it doesn't allow for tables implemented as BRAMs
	/*
	void Table::outputVHDL(std::ostream& o, std::string name) {

		licence(o);
			o << "library ieee; " << endl;
			o << "use ieee.std_logic_1164.all;" << endl;
			o << "library work;" << endl;
		outputVHDLEntity(o);
		newArchitecture(o,name);
		o << buildVHDLSignalDeclarations();

		// All the following is a generic table, hoping that the synthesis tools will do the job.
		// Xilinx-specific BRAM code generation can be found in the older versions, to be resurrected if needed
		int i,x;
		mpz_class y;
		beginArchitecture(o);
		o<<buildVHDLRegisters();
		o	<< "  with X select TableOut <= " << endl;
		REPORT(FULL,"Table.cpp: Filling the table");
		for (x = minIn; x <= maxIn; x++) {
			y=function(x);
			//if( y>=(1<<wOut) || y<0)
			//REPORT(0, "Output out of range" << "x=" << x << "  y= " << y );
			o<< tab << "\"" << unsignedBinary(y, wOut) << "\" when \"" << unsignedBinary(x, wIn) << "\"," << endl;
		}
		o << tab << "\"";
		for (i = 0; i < wOut; i++)
			o << "-";
		o <<  "\" when others;" << endl;

		// Delay the output properly
		o << tab << " Y <= TableOut";
		if(isSequential() && getPipelineDepth()!=0) {
			o << "_d" << getPipelineDepth();
		}
		o << ";" << endl;

		endArchitecture(o);
	}
	*/

	//old version -- just for testing
	void Table::outputVHDL(std::ostream& o, std::string name)
	{
		licence(o);
		o << "library ieee; " << endl;
		o << "use ieee.std_logic_1164.all;" << endl;
		o << "use ieee.numeric_std.all;" << endl;
		o << "library work;" << endl;
		outputVHDLEntity(o);
		newArchitecture(o,name);
		if (logicTable==1 || wIn <= getTarget()->lutInputs()){
			int i,x;
			mpz_class y;
			beginArchitecture(o);
			o	<< "  with X select  Y <= " << endl;
			REPORT(FULL,"Table.cpp: Filling the table");
			for (x = minIn; x <= maxIn; x++) {
				y=function(x);
				//if( y>=(1<<wOut) || y<0)
				//REPORT(0, "Output out of range" << "x=" << x << "  y= " << y );
				o<< tab << "\"" << unsignedBinary(y, wOut) << "\" when \"" << unsignedBinary(x, wIn) << "\"," << endl;
			}
			o << tab << "\"";
			for (i = 0; i < wOut; i++)
				o << "-";
			o <<  "\" when others;" << endl;
			//			Operator::outputVHDL(o,  name);
		}
		/* TODO The code below generates VHDL specific to one tool, one architecture, one FPGA...
	It is therefore currently unplugged, but it was probabaly added because it was improving performance. */
		else {
			int x;
			mpz_class y;

			o << tab << "-- Build a 2-D array type for the RoM" << endl;

			if (maxIn-minIn<=256 && wOut>36){
				o << tab << "subtype word_t is std_logic_vector("<< (wOut%2==0?wOut/2-1:(wOut+1)/2-1) <<" downto 0);" << endl;
				o << tab << "type memory_t is array(0 to 511) of word_t;" << endl;
			}else{
				o << tab << "subtype word_t is std_logic_vector("<< wOut-1 <<" downto 0);" << endl;
				o << tab << "type memory_t is array(0 to " << ((1<<wIn) -1) <<") of word_t;" << endl;
			}

			o << tab <<"function init_rom" << endl;
			o << tab << tab << "return memory_t is " << endl;
			o << tab << tab << "variable tmp : memory_t := (" << endl;

			int left = (wOut%2==0?wOut/2:(wOut+1)/2);
			int right= wOut - left;
			if (maxIn-minIn <= 256 && wOut>36 /* TODO Replace with target->getBRAMWidth */){
				/*special BRAM packing */
				//The first maxIn/2 go in the upper part of the table
				for (x = minIn; x <= maxIn; x++) {
					y=function(x);
					o << tab << "\"" << unsignedBinary(y>>right, (wOut%2==0?wOut/2:(wOut+1)/2)) << "\"," << endl;
				}
				for (x = maxIn; x < 255; x++) {
					o << tab << "\"" << unsignedBinary(0, (wOut%2==0?wOut/2:(wOut+1)/2)) << "\"," << endl;
				}
				for (x = minIn; x <= maxIn; x++) {
					y=function(x);
					y=y % (mpz_class(1) << right);
					o << tab << "\"" << unsignedBinary(y, (wOut%2==0?wOut/2:(wOut+1)/2)) << "\"," << endl;
				}
				for (x = maxIn; x < 255; x++) {
					o << tab << "\"" << unsignedBinary(0, (wOut%2==0?wOut/2:(wOut+1)/2)) << "\"," << endl;
				}
			}else{
				for (x = minIn; x <= maxIn; x++) {
					y=function(x);
					//if( y>=(1<<wOut) || y<0)
					//REPORT(0, "Output out of range" << "x=" << x << "  y= " << y );
					o << tab << "\"" << unsignedBinary(y, wOut) << "\"," << endl;
				}
			}

			o << tab << tab << "others => (others => '0'));" << endl;
			o << tab << tab << "	begin " << endl;
			o << tab << tab << "return tmp;" << endl;
			o << tab << tab << "end init_rom;" << endl;

			o << "	signal rom : memory_t := init_rom;" << endl;
			if (maxIn-minIn <= 256 && wOut>36){
				declare("Y1",(wOut%2==0?wOut/2:(wOut+1)/2));
				declare("Y0",(wOut%2==0?wOut/2:(wOut+1)/2));
				declare("Z1",9);
				declare("Z0",9);
			}else{
				declare("Y0",wOut);
			}

			outputVHDLSignalDeclarations(o);
			beginArchitecture(o);

			if (maxIn-minIn <= 256 && wOut>36){
				o << "Z0 <= " << zg(8-wIn) << (wIn>7 ? " &" : "") << " '1' & X;"<<endl;
				o << "Z1 <= " << zg(8-wIn) << (wIn>7 ? " &" : "") << " '0' & X;"<<endl;
			}

			if(isSequential()){
				o << "	process(clk)" << endl;
				o << tab << "begin" << endl;
				o << tab << "if(rising_edge(clk)) then" << endl;
			}
			if (maxIn-minIn <= 256 && wOut>36){
				o << tab << "	Y1 <= rom(  TO_INTEGER(unsigned(Z1)));" << endl;
				o << tab << "	Y0 <= rom(  TO_INTEGER(unsigned(Z0)));" << endl;
			}else{
				o << tab << "	Y0 <= rom(  TO_INTEGER(unsigned(X))  );" << endl;
			}
			if(isSequential()){
				o << tab << "end if;" << endl;
				o << tab << "end process;" << endl;
			}
			if (maxIn-minIn <= 256 && wOut>36){
				o << tab << " Y <= Y1 & Y0"<<range((wOut%2==0?wOut/2-1:(wOut-1)/2-1),0)<<";"<<endl;
			}else
				o << tab << " Y <= Y0;"<<endl;
		}

		endArchitecture(o);
	}


	int Table::size_in_LUTs() {
		return wOut*int(intpow2(wIn-getTarget()->lutInputs()));
	}

}
