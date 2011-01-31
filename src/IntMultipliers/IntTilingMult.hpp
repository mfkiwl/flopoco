#ifndef IntTillingMult_HPP
#define IntTillingMult_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <time.h>

#include "../Operator.hpp"
#include "../IntCompressorTree.hpp"
#include "../IntTruncMultiplier.hpp"
#include "LogicIntMultiplier.hpp"


namespace flopoco{
	/** The InttilingMult class */
	class IntTilingMult : public Operator
	{
	public:
		/**
		 * The FPAdder constructor
		 * @param[in]		target		the target device
		 * @param[in]		MSB			the MSB of the input number
		 * @param[in]		LSB			the LSB of the input number
		 * @param[in]		wER			the with of the exponent for the convertion result
		 * @param[in]		wFR			the with of the fraction for the convertion result
		 */
		IntTilingMult(Target* target, int wInX, int wInY,float ratio, int maxTimeInMinutes, bool interactive = true);
	
		

		/**
		 * IntTilingMult destructor
		 */
		~IntTilingMult();


		void emulate(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);
		
		void warningInfo(){
			REPORT(INFO, "********************************************************************************");
			REPORT(INFO, "*      The generation of this operator makes extensive use of the stack !      *");
			REPORT(INFO, "*      If operator generation is not successful, increase stack limit          *");
			REPORT(INFO, "*                    ulimit -s 5120000000                                      *");
			REPORT(INFO, "********************************************************************************");
		}

	private:
			
		void generateVHDLCode4CompleteTilling();
	
		/**
		 * Verifies that the indicated DSP block does not overlap with any 
		 * other block in the given configuration.
		 * @param config the tiling configuration containing DSP objects
		 * @param index the index of the DSP object we want to check is not overlapping in the configuration
		 * @return TRUE if no DSP blocks overlap on the tiling configuration
		 */
		bool checkOverlap(DSP** config, int index);
	
		/**
		 * Displaces the DSP block in the 'next' valid configuration, i.e.
		 * The DSP block in the configuration, indicated by the index parameter
		 * is moved in downward and to the left.
		 * @param config the tiling configuration containing DSP objects
		 * @param index the index of the DSP object we want to move within the configuration
		 * @return FALSE if we have reached the bottom-left most corner of the tiling
		 */
		bool move(DSP** config, int index);
	
		/**
		 * Repositions the indicated DSP object within the configuration in the 
		 * top-right most position possible.
		 * @param config the tiling configuration containing DSP objects
		 * @param index the index of the DSP object we want to move within the configuration
		 */
		bool replace(DSP** config, int index);
	
		/**
		 * Initializes the tiling grid with the DSP-s being positioned in the right-most
		 * and top-most position possible without any overlapping of the DSP blocks
		 * @param config the tiling configuration containing DSP objects
		 * @param dspCount the number of DSP blocks in the configuration
		 */
		void initTiling(DSP** &config, int dspCount);
		void initTiling2(DSP** &config, int dspCount);
		/**
		 * This function returns the amount of extra displacemnet on the tiling board that a dsp block can have on vertical axis
		 * @return extra height
		 */
		int getExtraHeight();

		/**
		 * This function returns the amount of extra displacemnet on the tiling
		 * board that a dsp block can have on horizonta axis
		 * @return extra width
		 */
		int getExtraWidth();
	
		/**
		 * This function tries to bind together as many multiplier blocks as
		 * possible into the same DSP block of the StratixII * architecture which has up to
		 * 3 adders
		 * @param the configuration of the tiling grid
		 * @return the number of operands for the final adder, which has been reduced from individual DSP count to sums of DSPs
		 */
		int bindDSPs4Stratix(DSP** config);

		/**
		 * This function generates the vhdl code for multiplications using DSP blocks, guided by the settings of the DSP objects.
		 * @param the configuration of the tiling grid
		 * @return the number of addition operands that contain DSP block multiplications
		 */
		int multiplicationInDSPs(DSP** config);
	 
	 	/**
		 * @param tx top-right x coordinate
		 * @param ty top-right y coordinate
		 * @param bx bottom-left x coordinate
		 * @param by bottom-left y coordinate
		 */
		void convertCoordinates(int &tx, int &ty, int &bx, int &by);

		void convertCoordinatesKeepNeg(int &tx, int &ty, int &bx, int &by);
	 
		/**
		 * This function generates the vhdl code for multiplication using only Slices.
		 * @param the configuration of the tiling grid
		 * @return the number of addition operands that use only Slices for multiplications
		 */
		int multiplicationInSlices(DSP** config);
	 
		/** The width of first input*/
		int wInX; 
		/** The width of second input*/
		int wInY; 
		/** The width of output */
		int wOut;
		/** The ratio between the number of DSPs and slices */
		float ratio;
		/** The truncation offset for the multiplication that are not completed. 0 value means that we perform a normal multiplication*/
		int truncationOffset;
		/** The working configuration of the tiling algorithm on DSPs */
		DSP** globalConfig;
		/** The best cost obtained so far*/
		float bestCost;
		/** The best configuration of the after tiling DSPs */
		DSP** bestConfig;
		/** The target */
//		Target * target; //FIXME Killed by Bogdan. Operator Class contains a pointer to this target, so a second one is useless 
		/** This will be used for computing the cost for each configuration */
		float costDSP,costLUT;
	
		/**Size of board minus extra bits*/
		int vnme,vmme;
		
		/** Used for partitioning the grid in smaller multiplications */
		int **mat;
	
		/**Used for creating a temporary storage of the current configuration in order to create binds between different DSPs */
		DSP** tempc;
	
		/** The vector which will mark if a dsp in the tiling algorithm was or not rotated */
		bool* rot;

		/** The number of estimated DSPs that will be used according to this parameter */
		int nrDSPs;
		
		/** TRUE if we are generating a squarer, FALSE if it is a simple multiplication */
		bool isSquarer;
		/** The width of the virtual board */
		int vn;
		/** The height of the virtual board */
		int vm;
		/** The maximum allow distance to move away from the others for the last block */
		int maxDist2Move;
	
		
		
		
		
		
		
		
		
		
		
		
		
		/** This function estimates the maximum number of DSPs that can be used
		with respect to the preference of the user */
		int estimateDSPs();

		/** This function computes the cost of the configuration received as input parameter */
	
		float computeCost(DSP** &config);

		/** This function compares the cost of the current configuration( globalConfig) with the best configuration obtained so far(bestConfig). If the current one is better then the best one, 
			 then it will copy the current configuration into the best one.
		*/
		void compareCost();
	
		/** This function is the backtracking function for finding the best configuration with respect to the user preference
		 */
		void tilingAlgorithm(int i, int n, bool repl,int lastMovedDSP);
	
		/** This function will take a configuration and will try to maximize the multiplications that are realized in slices 
		 * It will return the number of slices that are required to ferform those multiplications, and through the parameter partitions it will return the number of such partitions
		 */
		int partitionOfGridSlices(DSP** config,int &partitions);
	 
		/** This function will fill the input matrix between the limits received as parameters with the value received as parameter */
	 
		void fillMatrix(int **&matrix,int lw,int lh,int topleftX,int topleftY,int botomrightX,int botomrightY,int value);
	
		//~ /** This function resets all the conections that exists between DSPs of a configuration */
	
		//~ void resetConnections(DSP** &config);
	
		/** This function will create connections between the existing DSPs in the configuration according to the policies of Altera or Virtex */
	
		int bindDSPs(DSP** &config);
	
		/** This function will create the shifts between DSPs for Virtex boards */
	
		int bindDSPs4Virtex(DSP** &config);
	
		/** This function will sort the DSPs blocks by the topright corner */
	
		void sortDSPs(DSP** &config);
	
		/** This is a temporary function used only for displaing a configuration. It has no relevance to the algorithm. */
	
		void display(DSP** config);
	
		/** This function will be used to run the algorithm. the result of the algorithm (i..e. the best configuration) will be located in the member variable bestConfig */
	
		void runAlgorithm();
	
		/** This function is used in order to compare how much from the multiplication is occupied by the DSPs. It is used to compare the input parameter with the global maximum. 
			 It is used in the cases when the computed cost of the configuration is equal with the current maximum. This is neccessary because the Multiplication in LUT function reports same cost for
			 slightly different arguments in some cases.
		*/
	
		bool compareOccupation(DSP** config);
	
		/** This function is used in order to verify if the last DSP is not to far from the rest of the DSPs. This function should be called after we are sure that the last DSP is not in right of any others DSPs
			 @return 0 - for OK ; 1 - for to far left from all -> finnish ; 2 - is not above then any so move it left with 1 ; 3 continue to move normaly the block
		*/
	
		int checkFarness(DSP** config,int index);
		
		
		/** This functions splits large DSP super-blocks or paris of blocks into unit DSP blocks found on the target
		 * @param config - the configuration to be split
		 * @param numberOfDSPs - in-out parameter that holds the inital and final number of DSP blocks
		 * @return the split configuration
		 */
		DSP** splitLargeBlocks(DSP** config, int &numberOfDSPs);
		
		
		void outputVHDL(std::ostream& o, std::string name);
		
		void printConfiguration(DSP** configuration, vector<SoftDSP*> softDSPs);
		/** Functions used for Simulated Annealing */
		DSP** neighbour(DSP** config);
		float temp(float f);
		float probability(float e, float enew, float t);
		void simulatedAnnealing();
		int numberDSP4Overlap;
		//to be set in the InitTiling() with the appropriate value
		int nrOfShifts4Virtex;
		//int *countsShift;
		int dsplimit;
		int maxTimeInMinutes;
		clock_t start;
		clock_t finish;
	};
}

#endif