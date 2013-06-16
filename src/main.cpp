/*
  the FloPoCo command-line interface
  
  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon
  
  Authors : Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr
            Bogdan Pasca, Bogdan.Pasca@ens-lyon.org

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,  
  2008-2010.
  All rights reserved.

*/

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <mpfr.h>
#include <sollya.h>
#include <cstdlib>



#include "FloPoCo.hpp"


#define BRIGHT 1
#define RED 31
#define OPER 32
#define NEWOPER 32
#define PARAM 34
#define OP(op,paramList)             {cerr << "    "; printf("%c[%d;%dm",27,1,OPER); cerr <<  op; printf("%c[%dm",27,0); cerr<< " "; printf("%c[%d;%dm",27,1,PARAM); cerr << paramList; printf("%c[%dm\n",27,0); } 
#define NEWOP(op,paramList)          {cerr << "    "; printf("%c[%d;%dm",27,1,NEWOPER); cerr <<  op; printf("%c[%dm",27,0); cerr<< " "; printf("%c[%d;%dm",27,1,PARAM); cerr << paramList; printf("%c[%dm\n",27,0); } 


using namespace std;
using namespace flopoco;

// Global variables, useful in this main to avoid parameter passing


	string filename="flopoco.vhdl";
	string cl_name=""; // used for the -name option
	Target* target;
	
	//------------ Resource Estimation --------------------------------
	int reLevel;
	bool resourceEstimationDebug = false;
	//-----------------------------------------------------------------
	
	//------------------ Floorplanning --------------------------------
	bool floorplanning = false;
	bool floorplanningDebug = false;
	ostringstream floorplanMessages;
	//-----------------------------------------------------------------


extern void random_usage(char *name, string opName);
	
extern bool random_parseCommandLine(
	int argc, char *argv[], Target *target,
	std::string opname, int &i
);



void usage(char *name, string opName = ""){
	bool full = (opName=="");

	if ( full ){
		cerr << "\nUsage: "<<name<<" <operator specification list>\n" ;
		cerr << "Each operator specification is one of: \n";
	}
	if ( full || opName == "FixFunction"){					
		OP( "FixFunction","function a b target_accuracy");
		cerr << "      function - sollya-syntaxed function to implement, between double quotes\n";
	}

	if ( full || opName=="options"){
		cerr << "________________________________________________________________________________\n";
		cerr << "________________ OPTIONS________________________________________________________\n";
		cerr << "General options that should only appear before any operator specification:\n";
		cerr << "   -outputfile=<output file name>           (default=flopoco.vhdl)\n";
		cerr << "   -target=<Spartan3|Virtex4|Virtex5|Virtex6|StratixII|StratixIII|StratixIV|StratixV|CycloneII|CycloneIII|CycloneIV|CycloneV>      (default=Virtex5)\n";
		cerr << "Options affecting the operators that follow them:\n";
		cerr << "   -pipeline=<yes|no>                       (default=yes)\n";
		cerr << "   -frequency=<target frequency in MHz>     (default=400)\n";
		cerr << "   -clockenable=<yes|no>                    (default is no)\n";
		cerr << "   -DSP_blocks=<yes|no>\n";
		cerr << "       optimize for the use of DSP blocks   (default=yes)\n";
		cerr << "   -name=<entity name>\n";
		cerr << "       defines the name of the VHDL entity of the next operator\n";
		cerr << "   -resourceEstimation=level\n";
		cerr << "       level=0 disables resource estimation (default)\n";
		cerr << "       level=1..3 larger number means more details\n";
		cerr << "   -floorplanning=<yes|no>\n";
		cerr << "       generate a floorplan (experimental, Xilinx only)\n";
		cerr << "Debugging options, affecting the operators that follow them:\n";
		cerr << "   -verbose=<0|1|2|3>     verbosity level. 0: no output (default), 1: basic info, 3: full debug \n";
		cerr << "   -reDebugging=<yes|no>  debug output for resource estimation (default=no)\n";
		cerr << "   -flpDebugging=<yes|no> debug output for floorplanning (default=no)\n";
	}
	exit (EXIT_FAILURE);
}

int checkStrictlyPositive(char* s, char* cmd) {
	int n=atoi(s);
	if (n<=0){
		cerr<<"ERROR: got "<<s<<", expected strictly positive number."<<endl;
		usage(cmd);
	}
	return n;
}

int checkPositiveOrNull(char* s, char* cmd) {
	int n=atoi(s);
	if (n<0){
		cerr<<"ERROR: got "<<s<<", expected positive-or-null number."<<endl;
		usage(cmd);
	}
	return n;
}

bool checkBoolean(char* s, char* cmd) {
	int n=atoi(s);
	if (n!=0 && n!=1) {
		cerr<<"ERROR: got "<<s<<", expected a boolean (0 or 1)."<<endl;
		usage(cmd);
	}
	return (n==1);
}


int checkSign(char* s, char* cmd) {
	int n=atoi(s);
	if (n!=0 && n!=1) {
		cerr<<"ERROR: got "<<s<<", expected a sign bit (0 or 1)."<<endl;
		usage(cmd);
	}
	return n;
}

void addOperator(Operator *op) {
	if(cl_name!="")	{
		cerr << "Updating entity name to: " << cl_name << endl;
		op->changeName(cl_name);
		cl_name="";
	}
	op->addToGlobalOpList();
}



bool parseCommandLine(int argc, char* argv[]){
	if (argc<2) {
		usage(argv[0]); 
		return false;
	}

	Operator* op;
	int i=1;
	cl_name="";
	do {
		string opname(argv[i++]);
		if(opname[0]=='-'){
			string::size_type p = opname.find('=');
			if (p == string::npos) {
				cerr << "ERROR: Option missing an = : "<<opname<<endl; 
				return false;
			} else {
				string o = opname.substr(1, p - 1), v = opname.substr(p + 1);
				if (o == "outputfile") {
					if(!target->getGlobalOpListRef()->empty())
						cerr << "WARNING: global option "<<o<<" should come before any operator specification" << endl; 
					filename=v;
				}
				else if (o == "verbose") {
					verbose = atoi(v.c_str()); // there must be a more direct method of string
					if (verbose<0 || verbose>4) {
						cerr<<"ERROR: verbose should be 1, 2 or 3,    got "<<v<<"."<<endl;
						usage(argv[0], "options");
					}
				}
				else if (o == "target") {
					Target* oldTarget=target;
					if(!target->getGlobalOpListRef()->empty()){
								cerr<<"ERROR: target should be changed before any component is defined"<<endl; 
								usage(argv[0],"options");
					}
					if(v=="Virtex4") target=new Virtex4();
					else if (v=="Virtex5") target=new Virtex5();
					else if (v=="Virtex6") target=new Virtex6();
					else if (v=="Spartan3") target=new Spartan3();
					else if (v=="StratixII") target=new StratixII();
					else if (v=="StratixIII") target=new StratixIII();
					else if (v=="StratixIV") target=new StratixIV();
					else if (v=="StratixV") target=new StratixV();
					else if (v=="CycloneII") target=new CycloneII();
					else if (v=="CycloneIII") target=new CycloneIII();
					else if (v=="CycloneIV") target=new CycloneIV();
					else if (v=="CycloneV") target=new CycloneV();
					else {
						cerr<<"ERROR: unknown target: "<<v<<endl;
						usage(argv[0],"options");
					}
					// if previous options had changed it
					target->setFrequency(oldTarget->frequency());
					target->setUseHardMultipliers(oldTarget->hasHardMultipliers());
					if (oldTarget->isPipelined()) 
						target->setPipelined();
					else 
						target->setNotPipelined();
					target->setClockEnable(oldTarget->useClockEnable());
					delete(oldTarget);
				}

				else if (o == "pipeline") {
					if(v=="yes") target->setPipelined();
					else if(v=="no")  target->setNotPipelined();
					else {
						cerr<<"ERROR: pipeline option should be yes or no,    got "<<v<<"."<<endl; 
						usage(argv[0],"options");
					}
				}
				else if (o == "clockenable") {
					if(v=="yes") target->setClockEnable(true);
					else if(v=="no")  target->setClockEnable(false);
					else {
						cerr<<"ERROR: clockenable option should be yes or no,    got "<<v<<"."<<endl; 
						usage(argv[0],"options");
					}
				}
				else if (o == "frequency") {
					int freq = atoi(v.c_str());
					if (freq>1 && freq<10000) {
						target->setFrequency(1e6*(double)freq);
						if(verbose) 
							cerr << "Frequency set to "<<target->frequency()<< " Hz" <<endl; 
					}
					else {
						cerr<<"WARNING: frequency out of reasonible range, ignoring it."<<endl; 
					}
				}
				else if (o == "DSP_blocks") {
					if(v=="yes") target->setUseHardMultipliers(true);
					else if(v=="no")  target->setUseHardMultipliers(false);
					else {
						cerr<<"ERROR: DSP_blocks option should be yes or no,    got "<<v<<"."<<endl; 
						usage(argv[0],"options");
					}
				}
				else if (o == "name") {
					cl_name=v; // TODO?  check it is a valid VHDL entity name 
				}
			}
		}
		else if (opname == "FixFunction") {
			int nargs = 4;
			if (i+nargs > argc)
				usage(argv[0],opname); // and exit
			string func = argv[i++];
			double a = atof(argv[i++]);
			double b = atof(argv[i++]);
			double acc = atof(argv[i++]);
			// int wI = checkStrictlyPositive(argv[i++], argv[0]);
			// int wO = atoi(argv[i++]);
			// int n  = checkStrictlyPositive(argv[i++], argv[0]);
			FixFunction *toto = new FixFunction(func, a,b);
			toto -> buildPolyApprox(acc);
		}

		else  {
			cerr << "ERROR: Problem parsing input line, exiting";
			usage(argv[0]);
		}
	} while (i<argc);
	return true;
}

















int main(int argc, char* argv[] )
{
#ifdef HAVE_SOLLYA
	sollya_lib_init();
#endif

	uint32_t i;
	

	target = new Virtex5(); // this also creates a global operator list

	// for historical reasons, to get rid of some day

	try {
		parseCommandLine(argc, argv);
	} catch (char const * s) {
		cerr << "Exception while parsing command line: " << s << endl;
		return 1;
	} catch (std::string s){
		cerr << "Exception while parsing command line: " << s << endl;
		return 1;
	}

	 vector<Operator*>* oplist=target->getGlobalOpListRef();




	ofstream file;
	file.open(filename.c_str(), ios::out);
	Operator::outputVHDLToFile(*oplist, file); 
	file.close();
	
	cerr << endl<<"Final report:"<<endl;
	for(i=0; i<oplist->size(); i++) {
		(*oplist)[i]->outputFinalReport(0);
	}
	
	cerr<< "Output file: " << filename <<endl;
	

	//------------------------ Resource Estimation ---------------------
	for (vector<Operator*>::iterator it = oplist->begin(); it!=oplist->end(); ++it) {
		Operator* op = *it;
		
		if(reLevel!=0){
			if(op->reActive)
				cerr << op->generateStatistics(reLevel);
			else{
				cerr << "Resource estimation option active for an operator that has NO estimations in place." << endl;
			}
		}
	}
	//------------------------------------------------------------------
	
	//------------------ Resource Estimation Debugging -----------------
	if(resourceEstimationDebug){
		ofstream file;
		file.open("flopoco.debug");
		for (vector<Operator*>::iterator it = oplist->begin(); it!=oplist->end(); ++it) {
			Operator* op = *it;
			
			if(op->reActive)
				file << op->resourceEstimate.str();
			else{
				cerr << "Resource estimation debugging option active for an operator that has NO estimations in place." << endl;
			}
		}
		file.close();
		cerr << "Resource estimation log written to the \'flopoco.debug\' file" << endl;
	}
	//------------------------------------------------------------------
	
	//------------------------ Floorplanning ---------------------------
	if(floorplanning){
		for (vector<Operator*>::iterator it = oplist->begin(); it!=oplist->end(); ++it) {
			Operator* op = *it;
			
			if(op->reActive == false){
				cerr << "Floorplanning option can be used only when the resource estimations have been performed.\n" 
						<< " Please reconsider your strategy.\n" << endl;
				exit(1);
			}
			if(floorplanning)
				floorplanMessages << op->createFloorplan();
		}
	}
	//------------------------------------------------------------------
	
	//------------------ Floorplanning Debugging -----------------------
	if(floorplanningDebug){
		if(!(reLevel>0 && floorplanning)){
			cerr << "Debugging Floorplanning option can be used only when there are resource estimations and floorplanning is enabled." 
						<< " Please rerun the program with the appropriate options" << endl;
				exit(1);
		}else{
			ofstream file;
			file.open("flopoco.floorplan.debug");
			for (vector<Operator*>::iterator it = oplist->begin(); it!=oplist->end(); ++it) {
				Operator* op = *it;
				
				file << op->floorplan.str();
				file << floorplanMessages.str();
			}
			file.close();
			cerr << "Floorplanning log (for debugging purposes) written to the \'flopoco.floorplanning.debug\' file" << endl;
		}
	}


	//------------------------------------------------------------------

#ifdef HAVE_SOLLYA
	sollya_lib_close();
#endif

	return 0;
}



