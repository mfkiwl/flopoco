/*
 * The range reduction box for the FP logarithm
 * 
 * Author : Florent de Dinechin
 *
 * For a description of the algorithm, see the Arith17 paper. 
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
#include <fstream>
#include <math.h>
#include "../utils.hpp"
#include "LogRangeRed.hpp"
#include "../FPLog.hpp"

using namespace std;

extern vector<Operator*> oplist;


/** The constructor when LogRangeRed is instantiated from an FPLog
 * (who knows, other situations may arise) 
*/

LogRangeRed :: LogRangeRed(Target* target,
									FPLog *fplog
									): 
	Operator(target), 
	fplog(fplog),
	stages(fplog->stages),
	a(fplog->a), p(fplog->p), psize(fplog->psize), s(fplog->s)

{
	int i;
	ostringstream name; 
	name << fplog->getName() <<"_RangeRed" ;
	setName(name.str());

	setCopyrightString("F. de Dinechin (2008)");

	// Now allocate the various table objects


	it0 = new FirstInvTable(target, a[0], a[0]+1);
	oplist.push_back(it0);

	lt0 = new FirstLogTable(target, a[0], fplog->target_prec, it0);
	oplist.push_back(lt0);

	for(i=1;  i<= stages; i++) {
		lt[i] = new OtherLogTable(target, a[i], fplog->target_prec - p[i], i, a[i], p[i]); 
		oplist.push_back(lt[i]);
	 }


	addInput("Y0", fplog->wF+2);
	addInput("A", a[0]);
	addOutput("Z", s[stages+1]);
	addOutput("almostLog", lt0->wOut);


	vhdl << tab << declare("Y0c", fplog->wF+2) << " <= Y0;"<<endl;

	vhdl << tab << declare("A0", a[0]) << " <= A;"<<endl;

	vhdl << tab << "-- First inv table" << endl;
	inPortMap       (it0, "X", "A0");
	outPortMap      (it0, "Y", "InvA0");
	vhdl << instance(it0, "itO");



	nextCycle();

#define USE_FLOPOCO_INTMULT 1

#ifdef USE_FLOPOCO_INTMULT
	IntMultiplier* p0 = new IntMultiplier(target, a[0]+1, fplog->wF+2);
	oplist.push_back(p0);
	inPortMap  (p0, "X", "InvA0");
	inPortMap  (p0, "Y", "Y0c");
	outPortMap (p0, "R", "P0");
	vhdl << instance(p0, "p0_mult");
	setCycleFromSignal("P0", true);
#else
	vhdl << tab << declare("P0",  psize[0]) << " <= " << use("InvA0") << "*" << use("Y0c") << ";" <<endl <<endl;
#endif
	

	vhdl << tab << declare("Z1", s[1]) << " <= " << use("P0") << range (psize[0] - p[1]-3,  0) << ";" << endl;


	for (i=1; i<= stages; i++) {

		vhdl <<endl;
		//computation
		vhdl << tab << declare(join("A",i), a[i]) <<     " <= " << use(join("Z",i)) << range(s[i]-1,      s[i]-a[i]) << ";"<<endl;
		vhdl << tab << declare(join("B",i), s[i]-a[i]) << " <= " << use(join("Z",i)) << range(s[i]-a[i]-1, 0        ) << ";"<<endl;

		vhdl << tab << declare(join("ZM",i), psize[i]) << " <= " << use(join("Z",i)) ;
		if(psize[i] == s[i])
			vhdl << ";"<<endl;   
		else
			vhdl << range(s[i]-1, s[i]-psize[i])  << ";" << endl;   

		nextCycle();

#ifdef USE_FLOPOCO_INTMULT
		IntMultiplier* pi = new IntMultiplier(target, a[i], psize[i]);
		oplist.push_back(pi);
		inPortMap  (pi, "X", join("A",i));
		inPortMap  (pi, "Y", join("ZM",i));
		outPortMap (pi, "R", join("P",i));
		vhdl << instance(pi, join("p",i)+"_mult");
#else
		vhdl << tab << declare(join("P",i),  psize[i] + a[i]) << " <= " << join("A",i) << "*" << join("ZM",i) << ";" << endl;
		nextCycle();
#endif


#if 0
		vhdl << tab << declare(join("epsZ",i), s[i] + p[i] +2 ) << " <= " ;
		if(i==1) { // special case for the first iteration
			vhdl << 	rangeAssign(s[i]+p[i]+1,  0,  "'0'")  << " when  " << use("A1") << " = " << rangeAssign(a[1]-1, 0,  "'0'") << endl
				  << tab << "    else (\"01\" & "<< rangeAssign(p[i]-1, 0, "'0'") << " & " << use(join("Z",i)) << " )"
				  << "  when ((" << use("A1") << "("<< a[1]-1 << ")='0') and (" << use("A1") << range(a[1]-2, 0) << " /= " << rangeAssign(a[1]-2, 0, "'0'") << "))" << endl
				  << tab << "    else " << "(\"1\" & " << rangeAssign(p[i]-1, 0, "'0'") << " & " << use(join("Z",i)) << "  & \"0\") "
			  << ";"<<endl;
		}
		else {
			vhdl << rangeAssign(s[i]+p[i]+1,  0,  "'0'") 
				  << tab << "  when " << use(join("A",i)) << " = " << rangeAssign(a[i]-1,  0,  "'0'") << endl
				  << tab << "  else    (\"01\" & " << rangeAssign(p[i]-1,  0,  "'0'") << " & " << use(join("Z",i)) <<");"<<endl;
		}
		vhdl << tab << declare(join("Z", i+1), s[i+1]) << " <=   (\"0\" & " << use(join("B",i));
		if (s[i+1] > 1+(s[i]-a[i]))  // need to padd Bi
			vhdl << " & " << rangeAssign(s[i+1] - 1-(s[i]-a[i]) -1,  0 , "'0'");    
		vhdl <<")"<<endl 
			  << tab << "      - ( " << rangeAssign(p[i]-a[i],  0,  "'0'") << " & " << use(join("P", i));
		// either pad, or truncate P
		if(p[i]-a[i]+1  + psize[i]+a[i]  < s[i+1]) // size of leading 0s + size of p 
			vhdl << " & "<< rangeAssign(s[i+1] - (p[i]-a[i]+1  + psize[i]+a[i]) - 1,    0,  "'0'");  // Pad
		if(p[i]-a[i]+1  + psize[i]+a[i]  > s[i+1]) 
			//truncate
			vhdl << range(psize[i]+a[i]-1,    p[i]-a[i]+1  + psize[i]+a[i] - s[i+1]);
		vhdl << "  )"<< endl;
		
		vhdl << tab << "      + " << use(join("epsZ",i)) << range(s[i]+p[i]+1,  s[i]+p[i] +2 - s[i+1]) << ";"<<endl;
		



#else
		// While the multiplication takes place, we may prepare the other term 
		// nextCycle();
		
		int yisize = s[i]+p[i]+1; // +1 because implicit 1

		vhdl << tab << declare(join("Y",i), yisize) << " <= " 
			  << " \"1\" & " << rangeAssign(p[i]-1,  0,  "'0'") << " & " << use(join("Z",i)) <<";"<<endl;

		vhdl << tab << declare(join("epsY",i), s[i+1]) << " <= " 
			  << rangeAssign(s[i+1]-1,  0,  "'0'")  << tab << "  when " << use(join("A",i)) << " = " << rangeAssign(a[i]-1,  0,  "'0'") << endl
			  << tab << "  else ";
		if(2*p[i] - p[i+1] -1 > 0) 
			vhdl << rangeAssign(2*p[i] - p[i+1] - 2,  0,  "'0'")     // 2*p[i] - p[i+1] -1 zeros to perform multiplication by 2^2*pi
				  << " & " << use(join("Y",i)) << range(yisize-1,  yisize - s[i+1] +   2*p[i] - p[i+1] - 1 ) << ";" << endl;   
		//else 2*p[i] - p[i+1] -1 = 0, or there is something frankly wrong
		else
			vhdl << use(join("Y",i)) << range (yisize-1, yisize-s[i+1]) << ";" << endl;

		nextCycle();
		// IntAdder here?
		vhdl << tab << declare(join("epsYPB",i), s[i+1]) << " <= " 
			  << " (\"0\" & " << use(join("B",i));
		if (s[i+1] > 1+(s[i]-a[i]))  // need to padd Bi
			vhdl << " & " << rangeAssign(s[i+1] - 1-(s[i]-a[i]) -1,  0 , "'0'");    
		vhdl <<")"<<  " + " << use(join("epsY",i)) << ";" << endl; 

		syncCycleFromSignal(join("P",i), false);
		nextCycle();


#if 1 // simple addition
		vhdl << tab << declare(join("Pp", i), s[i+1])  << " <= " 
			  << rangeAssign(p[i]-a[i],  0,  "'0'") << " & " << use(join("P", i));
		// either pad, or truncate P
		if(p[i]-a[i]+1  + psize[i]+a[i]  < s[i+1]) // size of leading 0s + size of p 
			vhdl << " & "<< rangeAssign(s[i+1] - (p[i]-a[i]+1  + psize[i]+a[i]) - 1,    0,  "'0'");  // Pad
		if(p[i]-a[i]+1  + psize[i]+a[i]  > s[i+1]) 
			//truncate
			vhdl << range(psize[i]+a[i]-1,    p[i]-a[i]+1  + psize[i]+a[i] - s[i+1]);
		vhdl << ";"<<endl;

		vhdl << tab << declare(join("Z", i+1), s[i+1])  << " <= " 
			  << use(join("epsYPB",i)) << " - " << use(join("Pp", i)) << ";"<<endl;
#else
		// THIS CODE DOESN'T WORK -- look for the bug !	
		vhdl << tab << "--  compute epsYPBi - Pi" << endl;
		vhdl << tab << declare(join("mP", i), s[i+1])  << " <= " 
			  << " not (" << rangeAssign(p[i]-a[i],  0,  "'0'") << " & " << use(join("P", i));
		// either pad, or truncate P
		if(p[i]-a[i]+1  + psize[i]+a[i]  < s[i+1]) // size of leading 0s + size of p 
			vhdl << " & "<< rangeAssign(s[i+1] - (p[i]-a[i]+1  + psize[i]+a[i]) - 1,    0,  "'0'");  // Pad
		if(p[i]-a[i]+1  + psize[i]+a[i]  > s[i+1]) 
			//truncate
			vhdl << range(psize[i]+a[i]-1,    p[i]-a[i]+1  + psize[i]+a[i] - s[i+1]);
		vhdl << "  );"<<endl;
		IntAdder* ai = new IntAdder(target, s[i+1]);
		oplist.push_back(ai);
		inPortMap     (ai, "X", join("mP",i));
		inPortMap     (ai, "Y", join("epsY",i));
		inPortMapCst  (ai, "Cin", "'1'");
		outPortMap    (ai, "R", join("Z",i+1));
		vhdl << instance(ai, join("Z",i+1)+"adder");
#endif
		
#endif
	}

	
	vhdl << endl << tab << "-- Now the log tables, as late as possible" << endl;
	setCycle(getCurrentCycle() - stages -1 , true);


	vhdl << tab << "-- First log table" << endl;
	inPortMap       (lt0, "X", "A0");
	outPortMap      (lt0, "Y", "L0");
	vhdl << instance(lt0, "ltO");
	vhdl << tab << declare("S1", lt0->wOut) << " <= L0;"<<endl;
	nextCycle();

	for (i=1; i<= stages; i++) {

		// TODO better pipeline the small input as late as possible than pipeline the large output
		inPortMap       (lt[i], "X", join("A", i));
		outPortMap      (lt[i], "Y", join("L", i));
		vhdl << instance(lt[i], join("lt",i));
		nextCycle();
		vhdl << tab << declare(join("S",i+1),  lt0->wOut) << " <= " 
			  <<  use(join("S",i))  << " + (" << rangeAssign(lt0->wOut-1, lt[i]->wOut,  "'0'") << " & " << use(join("L",i)) <<");"<<endl;
	}

	vhdl << "   Z <= " << use(join("Z", stages+1)) << ";" << endl;  
	vhdl << "   almostLog <= " << use(join("S",stages+1)) << ";" << endl;  

} 


LogRangeRed::~LogRangeRed()
{
#if 0 // better done on oplist 
	delete it0;
	delete lt0;
	//delete it1;
	for(int i=1; i<=stages; i++) 
		delete lt[i];
#endif
}




