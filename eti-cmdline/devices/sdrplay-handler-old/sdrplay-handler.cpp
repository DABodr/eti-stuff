#
/*
 *    Copyright (C) 2014 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the eti-cmdline program
 *    eti-cmdline is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    eti-cmdline is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with eti-cmdline; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include	"sdrplay-handler.h"

#define	DEFAULT_GAIN	40

	sdrplayHandler::sdrplayHandler  (int frequency,
	                                 int16_t gain, bool autogain) {
int	err;
float	ver;

	this	-> frequency	= frequency;
	this	-> theGain	= gain;
	this	-> autogain	= autogain;

	this	-> inputRate	= Khz (2048);

	_I_Buffer	= NULL;
	libraryLoaded	= false;

#ifdef	__MINGW32__
HKEY APIkey;
wchar_t APIkeyValue [256];
ULONG APIkeyValue_length = 255;
	if (RegOpenKey (HKEY_LOCAL_MACHINE,
	                TEXT("Software\\MiricsSDR\\API"),
	                &APIkey) != ERROR_SUCCESS) {
          fprintf (stderr,
	           "failed to locate API registry entry, error = %d\n",
	           (int)GetLastError());
	   return;
	}
	RegQueryValueEx (APIkey,
	                 (wchar_t *)L"Install_Dir",
	                 NULL,
	                 NULL,
	                 (LPBYTE)&APIkeyValue,
	                 (LPDWORD)&APIkeyValue_length);
//	Ok, make explicit it is in the 64 bits section
	wchar_t *x = wcscat (APIkeyValue, (wchar_t *)L"\\x86\\mir_sdr_api.dll");
//	wchar_t *x = wcscat (APIkeyValue, (wchar_t *)L"\\x64\\mir_sdr_api.dll");
//	fprintf (stderr, "Length of APIkeyValue = %d\n", APIkeyValue_length);
//	wprintf (L"API registry entry: %s\n", APIkeyValue);
	RegCloseKey(APIkey);

	Handle	= LoadLibrary (x);
	if (Handle == NULL) {
	  fprintf (stderr, "Failed to open mir_sdr_api.dll\n");
	  throw (21);
	  return;
	}
#else
//	Ǹote that under Ubuntu, the Mirics shared object does not seem to be
//	able to find the libusb. That is why we explicity load it here
	Handle		= dlopen ("libusb-1.0.so", RTLD_NOW | RTLD_GLOBAL);

	Handle		= dlopen ("libmirsdrapi-rsp.so", RTLD_NOW);
	if (Handle == NULL)
	   Handle	= dlopen ("libmir_sdr.so", RTLD_NOW);

	if (Handle == NULL) {
	   fprintf (stderr, "error report %s\n", dlerror ());
	   throw (22);
	   return;
	}
#endif
	libraryLoaded	= true;

	loadFunctions ();
	if (!(loadFunctions ())) {
	   fprintf (stderr, " No success in loading sdrplay lib\n");
	   throw (22);
	}

	err			= my_mir_sdr_ApiVersion (&ver);
	if (ver != MIR_SDR_API_VERSION) {
	   fprintf (stderr, "Foute API: %f, %d\n", ver, err);
	}

	_I_Buffer	= new RingBuffer<DSPCOMPLEX>(2 * 1024 * 1024);
//
	running		= false;
}

	sdrplayHandler::~sdrplayHandler	(void) {
	stopReader ();
	if (_I_Buffer != NULL)
	   delete _I_Buffer;
}
//
static inline
int16_t	bankFor_sdr (int32_t freq) {
	if (freq < 10 * Khz (1))
	   return -1;
	if (freq < 12 * MHz (1))
	   return 1;
	if (freq < 30 * MHz (1))
	   return 2;
	if (freq < 60 * MHz (1))
	   return 3;
	if (freq < 120 * MHz (1))
	   return 4;
	if (freq < 250 * MHz (1))
	   return 5;
	if (freq < 420 * MHz (1))
	   return 6;
	if (freq < 1000 * MHz (1))
	   return 7;
	if (freq < 2000 * MHz (1))
	   return 8;
	return -1;
}

void	sdrplayHandler::setVFOFrequency	(int32_t newFrequency) {
mir_sdr_ErrT	err;
int32_t	realFreq = newFrequency;
int	gRdBSystem;
int	samplesPerPacket;

	if (bankFor_sdr (realFreq) == -1)
	   return;

	if (!running) {
	   frequency = newFrequency;
	   return;
	}

	if (bankFor_sdr (realFreq) == bankFor_sdr (frequency)) {
	   my_mir_sdr_SetRf (float (realFreq), 1, 0);
	   frequency	= realFreq;
	   return;
	}
	stopReader	();
	restartReader	();
}

int32_t	sdrplayHandler::getVFOFrequency	(void) {
	return frequency - vfoOffset;
}

int16_t	sdrplayHandler::maxGain	(void) {
	return 101;
}
//
//	For the setting of gain, not using a widget, we map the
//	gain value upon an attenation value and set setexternal Gain
void	sdrplayHandler::setGain		(int32_t g) {
	theGain		= g;
	my_mir_sdr_SetGr (maxGain () - theGain, 1, 0);
}

void	sdrplayHandler::setAgc	(bool b) {
	my_mir_sdr_AgcControl (b, - (maxGain () - theGain), 0, 0, 0, 0, 0);
	if (!b)
	   my_mir_sdr_SetGr (maxGain () - theGain, 1, 0);
}

static
void myStreamCallback (int16_t		*xi,
	               int16_t		*xq,
	               uint32_t		firstSampleNum, 
	               int32_t		grChanged,
	               int32_t		rfChanged,
	               int32_t		fsChanged,
	               uint32_t		numSamples,
	               uint32_t		reset,
	               void		*cbContext) {
int16_t	i;
sdrplayHandler	*p	= static_cast<sdrplayHandler *> (cbContext);
DSPCOMPLEX *localBuf = (DSPCOMPLEX *)alloca (numSamples * sizeof (DSPCOMPLEX));

	for (i = 0; i <  (int)numSamples; i ++)
	   localBuf [i] = DSPCOMPLEX (float (xi [i]) / 2048.0,
	                              float (xq [i]) / 2048.0);
	p -> _I_Buffer -> putDataIntoBuffer (localBuf, numSamples);
	(void)	firstSampleNum;
	(void)	grChanged;
	(void)	rfChanged;
	(void)	fsChanged;
	(void)	reset;
}

void	myGainChangeCallback (uint32_t	gRdB,
	                      uint32_t	lnaGRdB,
	                      void	*cbContext) {
	(void)gRdB;
	(void)lnaGRdB;	
	(void)cbContext;
}

bool	sdrplayHandler::restartReader	(void) {
int	gRdBSystem;
int	samplesPerPacket;
mir_sdr_ErrT	err;
int localGain	= maxGain () - theGain;

	if (running)
	   return true;

	err	= my_mir_sdr_StreamInit (&localGain,
	                                 double (inputRate) / MHz (1),
	                                 double (frequency) / Mhz (1),
	                                 mir_sdr_BW_1_536,
	                                 mir_sdr_IF_Zero,
	                                 0,	// lnaEnable do not know yet
	                                 &gRdBSystem,
	                                 autogain, // useGrAltMode,do not know yet
	                                 &samplesPerPacket,
	                                 (mir_sdr_StreamCallback_t)myStreamCallback,
	                                 (mir_sdr_GainChangeCallback_t)myGainChangeCallback,
	                                 this);
	if (err != mir_sdr_Success) {
	   fprintf (stderr, "Error %d on streamInit\n", err);
	   return false;
	}
//	my_mir_sdr_DebugEnable (1);

	err		= my_mir_sdr_SetDcMode (4, 1);
	err		= my_mir_sdr_SetDcTrackTime (63);
//
	my_mir_sdr_SetSyncUpdatePeriod ((int)(inputRate / 2));
	my_mir_sdr_SetSyncUpdateSampleNum (samplesPerPacket);
//	my_mir_sdr_AgcControl (1, -30, 0, 0, 0, 0, 0);
	my_mir_sdr_DCoffsetIQimbalanceControl (0, 1);
	running 	= true;
	return true;
}

void	sdrplayHandler::stopReader	(void) {
	if (!running)
	   return;

	my_mir_sdr_StreamUninit	();
	running		= false;
}

//
//	The brave old getSamples. For the mirics stick, we get
//	size still in I/Q pairs
//	Note that the sdrPlay returns 10 bit values
int32_t	sdrplayHandler::getSamples (DSPCOMPLEX *V, int32_t size) { 
	return _I_Buffer	-> getDataFromBuffer (V, size);
}

int32_t	sdrplayHandler::Samples	(void) {
	return _I_Buffer	-> GetRingBufferReadAvailable ();
}

void	sdrplayHandler::resetBuffer	(void) {
	_I_Buffer	-> FlushRingBuffer ();
}

int16_t	sdrplayHandler::bitDepth	(void) {
	return 12;
}

bool	sdrplayHandler::loadFunctions	(void) {

	my_mir_sdr_StreamInit	= (pfn_mir_sdr_StreamInit)
	                    GETPROCADDRESS (this -> Handle,
	                                    "mir_sdr_StreamInit");
	if (my_mir_sdr_StreamInit == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_StreamInit\n");
	   return false;
	}

	my_mir_sdr_StreamUninit	= (pfn_mir_sdr_StreamUninit)
	                    GETPROCADDRESS (this -> Handle,
	                                    "mir_sdr_StreamUninit");
	if (my_mir_sdr_StreamUninit == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_StreamUninit\n");
	   return false;
	}

	my_mir_sdr_SetRf	= (pfn_mir_sdr_SetRf)
	                    GETPROCADDRESS (Handle, "mir_sdr_SetRf");
	if (my_mir_sdr_SetRf == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_SetRf\n");
	   return false;
	}

	my_mir_sdr_SetFs	= (pfn_mir_sdr_SetFs)
	                    GETPROCADDRESS (Handle, "mir_sdr_SetFs");
	if (my_mir_sdr_SetFs == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_SetFs\n");
	   return false;
	}

	my_mir_sdr_SetGr	= (pfn_mir_sdr_SetGr)
	                    GETPROCADDRESS (Handle, "mir_sdr_SetGr");
	if (my_mir_sdr_SetGr == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_SetGr\n");
	   return false;
	}

	my_mir_sdr_SetGrParams	= (pfn_mir_sdr_SetGrParams)
	                    GETPROCADDRESS (Handle, "mir_sdr_SetGrParams");
	if (my_mir_sdr_SetGrParams == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_SetGrParams\n");
	   return false;
	}

	my_mir_sdr_SetDcMode	= (pfn_mir_sdr_SetDcMode)
	                    GETPROCADDRESS (Handle, "mir_sdr_SetDcMode");
	if (my_mir_sdr_SetDcMode == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_SetDcMode\n");
	   return false;
	}

	my_mir_sdr_SetDcTrackTime	= (pfn_mir_sdr_SetDcTrackTime)
	                    GETPROCADDRESS (Handle, "mir_sdr_SetDcTrackTime");
	if (my_mir_sdr_SetDcTrackTime == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_SetDcTrackTime\n");
	   return false;
	}

	my_mir_sdr_SetSyncUpdateSampleNum = (pfn_mir_sdr_SetSyncUpdateSampleNum)
	               GETPROCADDRESS (Handle, "mir_sdr_SetSyncUpdateSampleNum");
	if (my_mir_sdr_SetSyncUpdateSampleNum == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_SetSyncUpdateSampleNum\n");
	   return false;
	}

	my_mir_sdr_SetSyncUpdatePeriod	= (pfn_mir_sdr_SetSyncUpdatePeriod)
	                GETPROCADDRESS (Handle, "mir_sdr_SetSyncUpdatePeriod");
	if (my_mir_sdr_SetSyncUpdatePeriod == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_SetSyncUpdatePeriod\n");
	   return false;
	}

	my_mir_sdr_ApiVersion	= (pfn_mir_sdr_ApiVersion)
	                GETPROCADDRESS (Handle, "mir_sdr_ApiVersion");
	if (my_mir_sdr_ApiVersion == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_ApiVersion\n");
	   return false;
	}

	my_mir_sdr_AgcControl	= (pfn_mir_sdr_AgcControl)
	                GETPROCADDRESS (Handle, "mir_sdr_AgcControl");
	if (my_mir_sdr_AgcControl == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_AgcControl\n");
	   return false;
	}

	my_mir_sdr_Reinit	= (pfn_mir_sdr_Reinit)
	                GETPROCADDRESS (Handle, "mir_sdr_Reinit");
	if (my_mir_sdr_Reinit == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_Reinit\n");
	   return false;
	}

	my_mir_sdr_SetPpm	= (pfn_mir_sdr_SetPpm)
	                GETPROCADDRESS (Handle, "mir_sdr_SetPpm");
	if (my_mir_sdr_SetPpm == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_SetPpm\n");
	   return false;
	}

	my_mir_sdr_DebugEnable	= (pfn_mir_sdr_DebugEnable)
	                GETPROCADDRESS (Handle, "mir_sdr_DebugEnable");
	if (my_mir_sdr_DebugEnable == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_DebugEnable\n");
	   return false;
	}

	my_mir_sdr_DCoffsetIQimbalanceControl	=
	                     (pfn_mir_sdr_DCoffsetIQimbalanceControl)
	                GETPROCADDRESS (Handle, "mir_sdr_DCoffsetIQimbalanceControl");
	if (my_mir_sdr_DCoffsetIQimbalanceControl == NULL) {
	   fprintf (stderr, "Could not find mir_sdr_DCoffsetIQimbalanceControl\n");
	   return false;
	}
	return true;
}

