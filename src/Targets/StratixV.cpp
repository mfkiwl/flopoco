/*
  A model of Stratix V FPGA

  Authors: Florent de Dinechin, Matei Istoan, Sebastian Banescu

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon
  
  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,  
  2008-2012.
  All rights reserved.

*/
#include "StratixV.hpp"
#include <iostream>
#include <sstream> 
#include "../utils.hpp"


namespace flopoco{
	
	double StratixV::adderDelay(int size) {
		int subAdd = 0;
		
		suggestSubaddSize(subAdd, size);
		
		return (
			lutDelay_ + 
			((size-1-ceil((2*size/almsPerLab_)/2.0)-(size/almsPerLab_)) * fastcarryDelay_) + 
			(ceil((2*size/almsPerLab_)/2.0) * innerLABcarryDelay_) + 
			((size/almsPerLab_) * interLABcarryDelay_) + 
			carryInToSumOut_ + 
			(size/subAdd)  * (ffDelay_ + fdCtoQ_)
		);
	};
	
	void StratixV::getAdderParameters(double &k1, double &k2, int size)
	{
		int subAdd = 0;
		
		suggestSubaddSize(subAdd, size);
		
		k1 = lutDelay_ + carryInToSumOut_;
		k2 = double(
						((size-1-ceil((2*size/almsPerLab_)/2.0)-(size/almsPerLab_)) * fastcarryDelay_) + 
						(ceil((2*size/almsPerLab_)/2.0) * innerLABcarryDelay_) + 
						((size/almsPerLab_) * interLABcarryDelay_) + 
						(size/subAdd)  * (ffDelay_ + fdCtoQ_)
			) / double(size-1); 
	};
	
	double StratixV::eqComparatorDelay(int size){
		return adderDelay(size) + elemWireDelay_ + lut2_; // TODO: check correctness
	};
	
	double StratixV::eqConstComparatorDelay(int size){
		return adderDelay(size/2) + elemWireDelay_ + lut2_; // // TODO: check correctness
	};
	
	double StratixV::carryPropagateDelay() {
		return  fastcarryDelay_; 
	};
	
	double StratixV::localWireDelay(int fanout){
		return lut2lutDelay_;
	};
	
	double StratixV::distantWireDelay(int n){
		return n*elemWireDelay_;
	};
	
	double StratixV::lutDelay(){
		return lutDelay_;
	};
	
	double StratixV::ffDelay(){
		return ffDelay_;
	};
	
	long StratixV::sizeOfMemoryBlock()
	{
		return sizeOfBlock_;	
	};
	
	DSP* StratixV::createDSP() 
	{
		int multW, multH;
		getDSPWidths(multW, multH);
		
		// create a DSP block having a shifting amount of 0
		DSP* dsp_ = new DSP(0, multW, multH);
		
		return dsp_;
	};
	
	bool StratixV::suggestSubmultSize(int &x, int &y, int wInX, int wInY){
		// (DSP blocks are 36x36 and my be split as 9x9, 12x12 or 18x18)
			int padX[nrConfigs_+1], padY[nrConfigs_+1], ix=1, iy=1; // nr of zero padding for a specific width multiplier
			double maxF[nrConfigs_+1]; // will hold the maximum possible freqeuncy for each multiplier width
			
			x = y = 1;
			padX[nrConfigs_] = wInX-x;
			padY[nrConfigs_] = wInY-y;
			maxF[nrConfigs_] = 0;
			
			for (int i=0; i<nrConfigs_; i++)
			{ // for each multiplier width available
			maxF[i] = 1/multiplierDelay_[i]; // maximum possible freqeuncy 
			int chunksX = ceil((double)wInX/multiplierWidth_[i]);
			int chunksY = ceil((double)wInY/multiplierWidth_[i]);
			padX[i] = chunksX*multiplierWidth_[i] - wInX;
			padY[i] = chunksY*multiplierWidth_[i] - wInY;
			
			cout << "Mult Size: " << multiplierWidth_[i] << ", Chunks X: " << chunksX << ", Y: " << chunksY << ", PaddX: "<<padX[i] << ", PaddY: " << padY[i] << endl;
			if (frequency() > maxF[i]) 
				continue;
			
			
			if ((padY[i] < (multiplierWidth_[i]/ (double)y)*padY[nrConfigs_]) ||
				((padY[i] == (multiplierWidth_[i]/ (double)y)*padY[nrConfigs_]) &&
				(multiplierWidth_[i] > multiplierWidth_[iy])))
			{
				y = multiplierWidth_[i];
				padY[nrConfigs_] = padY[i];
				iy = i;
			}
			
			if ((padX[i] < (multiplierWidth_[i]/ (double)x)*padX[nrConfigs_]) ||
				((padX[i] == (multiplierWidth_[i]/ (double)x)*padX[nrConfigs_]) &&
				(multiplierWidth_[i] > multiplierWidth_[ix])))
			{
				x = multiplierWidth_[i];
				padX[nrConfigs_] = padX[i];
				ix = i;
			}	
			
			if (x < y)
			{
				y = x;
				iy = ix;
				padY[nrConfigs_] = padY[iy];
			}
			else if (y < x)
			{
				x = y;
				ix = iy;
				padX[nrConfigs_] = padX[ix];
			}
			cout << "x: " << x << ", y: " << y << ", padX: " << padX[nrConfigs_] << ", padY: " << padY[nrConfigs_] << endl;
			}
			
			if ((x != 1) && (y != 1))
			{
				int maxFx = 1/multiplierDelay_[ix];
				int maxFy = 1/multiplierDelay_[iy]; 
				int wIn;
				
				if (maxFx>maxFy)
				{
					maxF[nrConfigs_] = maxFy;
					wIn = y;
				}
				else
				{
					maxF[nrConfigs_] = maxFx;
					wIn = x;
				}
				
				if (frequency() < maxF[nrConfigs_])
				{
					double f = frequency();
					
					if ((f > 1./multiplierDelay_[2] && wIn == 18) || 	// don't use 36x36
						(wIn == 12) ||									// don't have 24x24
						(wIn > 18)) 	
						return false;
					else
						return true;
				}
				else
				{
					x = y = 18;
					return false;
				}
			}
			else // the desired frequency is too high
			{
				x = y = 18;
				return false;
			}
	}	 
	
	bool StratixV::suggestSubaddSize(int &x, int wIn){
		
		suggestSlackSubaddSize(x, wIn, 0);
		
		if (x>0) 
			return true;
		else 
		{
			x=1;		
			return false;
		} 
	}
	
	bool  StratixV::suggestSlackSubaddSize(int &x, int wIn, double slack){
		
		float time = 1./frequency() - slack - (lutDelay_ + carryInToSumOut_);
		int chunkSize = 0;
		
		while (time > 0)
		{
			chunkSize++;
			
			if ((chunkSize % (almsPerLab_*2) == 0) && (chunkSize % almsPerLab_ != 0))
			{
				time -= innerLABcarryDelay_;
			}else if (chunkSize % almsPerLab_ == 0)
			{
				time -= interLABcarryDelay_;
			}else
			{
				if(chunkSize != 0)
					time -= fastcarryDelay_;
			}
		}
		
		if(time<0)
			chunkSize--; // decremented because of the loop condition (time > 0). When exiting the loop the time is negative
		
		x = chunkSize;		
		if (x>0)
		{ 
			return true;
		}else 
		{
			x=1;		
			return false;
		} 
	}
	
	bool StratixV::suggestSlackSubcomparatorSize(int& x, int wIn, double slack, bool constant)
	{
		bool succes = true;
		float time = 1.0/frequency();
		unsigned int count;
		
		while(time > eqComparatorDelay(wIn))
			count++;
			
		if(time < eqComparatorDelay(count))
			count--;
		
		x = count;
		if (x>0)
		{ 
			return true;
		}else 
		{
			x=1;		
			return false;
		}
	}
	
	int StratixV::getIntMultiplierCost(int wInX, int wInY){
		
		int cost = 0;
		int halfLut = lutInputs_/2;
		int cx = int(ceil((double) wInX/halfLut));
		int cy = int(ceil((double) wInY/halfLut));
		if (cx > cy) // set cx as the min and cy as the max
		{
			int tmp = cx;
			cx = cy;
			cy = tmp;
		}
		
		float p = (double)cy/(double)halfLut; // number of chunks concatenated per operand
		float r = p - floor(p); // relative error; used for detecting how many operands have ceil(p) chunks concatenated
		int chunkSize, lastChunkSize, nr, aux;
		suggestSubaddSize(chunkSize, wInX+wInY);
		lastChunkSize = (wInX+wInY)%chunkSize;
		nr = ceil((double) (wInX+wInY)/chunkSize);
		
		
		if (r == 0.0) // all IntNAdder operands have p concatenated partial products
		{
			aux = halfLut*cx;
			
			cost = p*lutInputs_*(aux-2)*(aux-1)/2; // registered partial products without zero paddings
		}
		else if (r > 0.5) // 2/3 of the IntNAdder operands have p concatenated partial products
		{
			aux = (halfLut-1)*cx;
			
			cost = ceil(p)*lutInputs_*(aux-2)*(aux-1)/2 + floor(p)*lutInputs_*((aux*cx)+(cx-2)*(cx-1)/2);// registered partial products without zero paddings
		}
		else if (r > 0) // 1/3 of the IntNAdder operands have p concatenated partial products
		{
			aux = (halfLut-1)*cx;
			
			cost = ceil(p)*lutInputs_*(cx-2)*(cx-1)/2 + floor(p)*lutInputs_*((aux*cx)+(aux-2)*(aux-1)/2);// registered partial products without zero paddings
		}
		
		aux = halfLut*cx;
		cost += p*lutInputs_*aux + halfLut*(aux-1)*aux/2; // registered addition results on each pipeline stage of the IntNAdder
		cost += (nr-1)*(wInX+wInY) + (nr-1)*nr/2 + nr*lastChunkSize + nr*(nr-1)*(chunkSize+1)/2; // final IntAdder cost LUT + Registers
		cost += cx*cy*lutInputs_*2; // LUT cost for small multiplications 
		
		return cost/2;
	}
	
	//TODO: give the meaning for sign
	//		check validity
	void StratixV::getDSPWidths(int &x, int &y, bool sign)
	{ 
	
		if (sign == false)
			x = y = 18;
		else
			x = y = 18;
		
		// set the multiplier width acording to the desired frequency
		for (int i=0; i<nrConfigs_; i++)
			if (frequency_ < 1.0/multiplierDelay_[i])
			{
				x = multiplierWidth_[i];
				y = multiplierWidth_[i];
				
			}
	}
	
	int StratixV::getEquivalenceSliceDSP(){
		int lutCost = 0;
		int x, y;
		getDSPWidths(x,y);
		// add multiplier cost
		lutCost += getIntMultiplierCost(x, y);
		// add accumulator cost
		//lutCost += accumulatorLUTCost(x, y);
		// add partial cost of final adder
		//lutCost += adderLUTCost(x,y);
		return lutCost;
	}
	
	int StratixV::getNumberOfDSPs() 
	{
		int x, y;
		getDSPWidths(x, y);
		
		switch (x)
		{
			case 9: 
					y = totalDSPs_*8;
					break;
			case 16: 
					y = totalDSPs_*4;
					break;
			case 18: 
					y = totalDSPs_*4;
					break;
			case 27: 
					y = totalDSPs_*3;
					break;
			case 36: 
					y = totalDSPs_*2;
					break;
		}
		return y;		
	};
	
	int StratixV::getIntNAdderCost(int wIn, int n)
	{
		int chunkSize, lastChunkSize, nr, a, b, cost;
		
		suggestSubaddSize(chunkSize, wIn);
		lastChunkSize = wIn%chunkSize;
		nr = ceil((double) wIn/chunkSize);
		a = (nr-1)*wIn + (nr-1)*nr/2 + wIn*((n-1)*n/2-1)+nr*(n-2);
		b = nr*lastChunkSize + nr*(nr-1)*(chunkSize+1)/2 + nr*(n-1) + (n-2)*wIn;
		cost = (a+b)/2;
		return cost;
	}
	
	
}