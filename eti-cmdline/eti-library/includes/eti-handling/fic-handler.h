#
/*
 *    Copyright (C) 2013
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the eti-frontend
 *    eti-frontend is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    eti-frontend is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with eti-frontend; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#
/*
 * 	FIC data
 */
#ifndef	__FIC_HANDLER__
#define	__FIC_HANDLER__

#include	<stdio.h>
#include	<stdint.h>
#include	"viterbi.h"
#include	"fib-processor.h"

class	dabParams;
class	ensembleHandler;

class ficHandler:public viterbi {
public:
			ficHandler		(dabParams  *,
	                                         ensembleHandler *);
			~ficHandler		(void);
	void		process_ficBlock	(int16_t *, uint8_t *, bool *);
	void		clearEnsemble		(void);
	int16_t		get_ficRatio		(void);
	void		get_channelInfo		(channel_data *, int);
	void		get_CIFCount		(int16_t *, int16_t *);
private:
	fib_processor	fibProcessor;
	void		process_ficInput	(int16_t *,
	                                         int16_t, uint8_t *, bool *);
	int8_t		*PI_15;
	int8_t		*PI_16;
	uint8_t		*bitBuffer_in;
	uint8_t		*bitBuffer_out;
	int16_t		*ofdm_input;
	int16_t		index;
	int16_t		BitsperBlock;
	int16_t		ficno;
	int16_t		ficBlocks;
	int16_t		ficMissed;
	int16_t		ficRatio;
	uint16_t	convState;
	uint8_t		PRBS [768];
	uint8_t		shiftRegister [9];
};

#endif


