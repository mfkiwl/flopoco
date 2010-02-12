/*
 * Table Generator unit for FloPoCo
 *
 * Author : Mioara Joldes
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

#ifdef HAVE_SOLLYA

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <math.h>
#include <string.h>
#include <gmp.h>
#include <mpfr.h>
#include <cstdlib>
#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"

#include "TableGenerator.hpp"


using namespace std;

namespace flopoco{

	extern vector<Operator*> oplist;

	TableGenerator::TableGenerator(Target* target, int wIn, int wOut ): // TODO extend list
		Operator(target), wIn_(wIn), wOut_(wOut)  {
	
		ostringstream name;
		/* Set up the name of the entity */
		name <<"TableGenerator_"<<wIn<<"_"<<wOut;
		setName(name.str());
	
		/* Set up the I/O signals of of the entity */
		addInput("I", wIn);
		addOutput("O", wOut);

		/* This operator is combinatorial (in fact is just a ROM.*/
		setCombinatorial();
	
		/* Convert the input string into a sollya evaluation tree */
		
		sollya_node_t sF = parseString("sin(x)");
		
		sollya_node_t tempNode, tempNode2, tempNode3;
  sollya_chain_t tempChain;
  mpfr_t a;
  mpfr_t b;
  
   tempNode = parseString("1/log(1 + x)");
  tempNode2 = parseString("1");

  tempChain = makeIntPtrChainFromTo(1,5);

  mpfr_init2(a,12);
  mpfr_init2(b,12);

  mpfr_set_d(a,-0.25,GMP_RNDN);
  mpfr_set_d(b,0.25,GMP_RNDN);

  tempNode3 = remez(tempNode2, tempNode, tempChain, a, b, NULL, getToolPrecision());

  mpfr_clear(a);
  mpfr_clear(b);


  printTree(tempNode3);
  printf("\n");

  free_memory(tempNode);
  free_memory(tempNode2);
  free_memory(tempNode3);

  freeChain(tempChain,freeIntPtr);

  /* Second example that calls a error recovery function */

  tempNode = parseString("1/log(1 + x)");
  tempNode2 = parseString("1");

  tempChain = makeIntPtrChainFromTo(0,5);

  mpfr_init2(a,12);
  mpfr_init2(b,12);

  mpfr_set_d(a,-0.25,GMP_RNDN);
  mpfr_set_d(b,0.25,GMP_RNDN);

  tempNode3 = remez(tempNode2, tempNode, tempChain, a, b, NULL, getToolPrecision());

  mpfr_clear(a);
  mpfr_clear(b);


  printTree(tempNode3);
  printf("\n");

  free_memory(tempNode);
  free_memory(tempNode2);
  free_memory(tempNode3);

  freeChain(tempChain,freeIntPtr);

	}

	TableGenerator::~TableGenerator() {
	}

}

#endif //HAVE_SOLLYA