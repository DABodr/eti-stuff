#
/*
 *    Copyright (C) 2015
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the SDR-J (JSDR).
 *    SDR-J is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    SDR-J is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with SDR-J; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#
#ifndef	__MSC_DATAGROUP
#define	__MSC_DATAGROUP

#include	"dab-processor.h"
#include	"ringbuffer.h"
#include	<stdio.h>
#include	<string.h>


class	RadioInterface;
class	virtual_dataHandler;

class	mscDatagroup: public dabProcessor {
public:
	mscDatagroup	(RadioInterface *mr,
	                 uint8_t	DSCTy,
	                 int16_t	packetAddress,
	                 int16_t	bitRate,
	                 uint8_t	DGflag,
	                 int16_t	FEC_scheme);
	~mscDatagroup	(void);
int32_t	process		(int16_t *);
void	stopRunning	(void);
private:
	RadioInterface	*myRadioInterface;
	uint8_t		DSCTy;
	int16_t		packetAddress;
	int16_t		fragmentSize;
	int16_t		bitRate;
	uint8_t		DGflag;
	int16_t		FEC_scheme;
	int16_t		crcErrors;
	int16_t		handledPackets;
	FILE		*tstFile;
	uint8_t		*outV;
	int16_t		*Data;
	QByteArray	series;
	uint8_t		packetState;
	int32_t		streamAddress;		// int since we init with -1
//
//	result handlers
	void		handleTDCAsyncstream 	(uint8_t *, int16_t);
	void		handlePackets		(uint8_t *, int16_t);
	void		handlePacket		(uint8_t *);
	virtual_dataHandler *my_dataHandler;
};

#endif

