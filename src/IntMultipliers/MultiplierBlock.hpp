#ifndef MultiplierBlock_HPP
#define MultiplierBlock_HPP
#include <stdio.h>
#include <stdlib.h>
#include "../Operator.hpp"
#include <vector>
#include <sstream>
#include <gmp.h>
#include <gmpxx.h>
#include "utils.hpp"
#include "Table.hpp"


namespace flopoco {

	//FIXME: please comment your code!

	class MultiplierBlock 
	{
	public: 
		
		class SmallMultTable: public Table 
		{
			public:
			
			int dx, dy, wO;
			bool signedIO;
			SmallMultTable(Target* target, int dx, int dy, int wO, bool signedIO=false );
			mpz_class function(int x);
		};
	
		/**
		 * The default constructor
		 */
		MultiplierBlock(int wX, int wY, int topX, int topY, string input1, string input2, int weightShift = 0, int cycle = -1);
	
		
		/**
		 * Set the value of signalName
		 * @param the new value
		 */
		void setSignalName(string name);

		void setSignalLength(int length);

		int getWeight()
		{
			return weight;
		}

		string getSigName();

		int getSigLength();

		int getwX();

		int getwY();

		int gettopX();

		int gettopY();

		int getbotX();
		
		int getbotY();

		int getCycle();

		string getInputName1()
		{return inputName1;}

		
		string getInputName2()
		{return inputName2;}
		
		bool canBeChained(MultiplierBlock* next);

		MultiplierBlock* getNext();

		MultiplierBlock* getPrevious();

		void setNext(MultiplierBlock* b);

		void setPrevious(MultiplierBlock* b);
		
		string PP(int i, int j, int nr=-1);
		string PPTbl( int i, int j, int nr=-1);
		string XY(int i, int j, int nr=-1);
		string heap( int i, int j);

		void generateVHDLforDSP(int nr,int i);
		void generateVHDLforLOGIC(int nr);
		void generateVHDL(int nr, int i);
	

	bool operator <= (MultiplierBlock* b); 

		
	private:
	
		int wX; 							/**< x size */
		int wY; 							/**< y size */
		int topX; 							/**< x position (top right corner) */
		int topY; 							/**< y position (top right corner */
		int cycle;							/**< cycle */
		MultiplierBlock* previous;
		MultiplierBlock* next;
		string signalName;
		int signalLength;
		int weight;
		int weightShift;
		string inputName1, inputName2;
	};

}
#endif
