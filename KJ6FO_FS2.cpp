
//
// Flying Squirrel #2 flight computer software
//
// Author Don Gibson KJ6FO

// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject
// to the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
// CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
#define DEBUGMESSAGES 1
#define DEBUGSCHEDULE 1

 
#include "si5351.h"

#include "KJ6FOWSPR.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include<iostream>
#include <chrono>
#include <time.h>

//#define CALIBRATIONADJUST  -18627L // Measured from calibration program.  Proto board 1
#define CALIBRATIONADJUST  -7800L // Measured from calibration program. FS2 Flight Hardware Board #2

// Mode defines

//
// WSPR
//
#define WSPR_DEFAULT_FREQ       14095600UL + 1400UL  //Hz  Base(Dial) Freq plus audio offset to band pass bottom edge.
#define WSPR_DEFAULT_FREQ_ADJ         40L // 40Hz up from bottom edge of 200hz passband 
#define WSPR_TONE_SPACING       14648+5   // ~1.4648 Hz + 5 to handle interger rounding.
#define WSPR_TONE_DIVISOR       100       // Divisor to bring the number to 1.46-ish
#define WSPR_DELAY              682667UL     // Delay nS value for WSPR .682667s  minus time it takes to change frequency (Measured varies by CPU board, but 5 seems to work weel for most)
#define WSPR_POWER_DBM				13    // 13 dbm = 20 Milliwatts          
#define WSPRType1 1  // Should be  enums, but having compiler issues
#define WSPRType2 2
#define WSPRType3 3


//
// FSQ
//
#define FSQ_DEFAULT_FREQ        (14097000UL + 1350UL - 80UL)    // HZ Base freq is 1350 Hz higher than dial freq in USB  //Board2
#define FSQ_TONE_SPACING        879          // ~8.79 Hz
#define FSQ_TONE_DIVISOR        1            // Divisor of 1 will leave TONE_SPACING unchanged
#define FSQ_2_DELAY             500000UL         // nS Delay value for 2 baud FSQ
#define FSQ_3_DELAY             333000UL         // nS Delay value for 3 baud FSQ
#define FSQ_4_5_DELAY           222000UL         //nS Delay value for 4.5 baud FSQ
#define FSQ_6_DELAY             167000UL         // nS Delay value for 6 baud FSQ


//
// Hardware defines
//
#define OFF 0
#define ON 1



//
// Global Class instantiations
//
Si5351 si5351;
KJ6FOWSPR jtencode;

//
// Global variables
//

char MyCallsign[] = "KA9CQL/B";

uint8_t RadioPower_dbm = WSPR_POWER_DBM; 
#define TXBUFFERSIZE 512
uint8_t tx_buffer[TXBUFFERSIZE+1];


//
// Global GPS Info
//
// Current values of time and position etc.
float LastLat = 34.492787;  // Last reported Lat
float LastLon = -117.407607;  // Last reported Long
float CurrLat = LastLat;  // Current Lat
float CurrLon = LastLon;  // Current Long
float CurrAlt = 982.90;    // Current Altitude
char CurrentGridSquare[7] = "DM15hl";  // 6 digit grid square
float CurrentTemp1 = 25; // Degrees C
float CurrentTemp2 = 25; // Degrees C
volatile int CurrFix = 1; //  Fix number (i.e. # of fixes from GPS)
volatile int CurrDays = 1; //  # of Days
volatile int CurrHours = 1; //  # of Hours
volatile int CurrMinutes = 1;
int LastSecond = -1;   // used by time debug messages in loop() 

bool bRadioIsOn = false;



///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////// CODE ////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

//
// Turn Radio Power switch on/off.  
//
void RadioPower(int OnOff)
{
	printf("R PWR %s\n", (OnOff ? "ON" : "OFF"));
	//digitalWrite(RADIOPOWER_PIN, OnOff);
	bRadioIsOn = OnOff;
}


// GridSquare6Digits should be 7 bytes long to allow for null terminator.
void Calc6DigitGridSquare(char *GridSquare6Digits, double lat, double lon)
{
	int o1, o2, o3;
	int a1, a2, a3;
	double remainder;

	// DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG
	//lat += 2.0; // Delibrate error to test moving grid squares
	//lon += 1.0;
	// DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG


	// longitude
	remainder = lon + 180.0;
	o1 = (int)(remainder / 20.0);
	remainder = remainder - (double)o1 * 20.0;
	o2 = (int)(remainder / 2.0);
	remainder = remainder - 2.0 * (double)o2;
	o3 = (int)(12.0 * remainder);

	// latitude
	remainder = lat + 90.0;
	a1 = (int)(remainder / 10.0);
	remainder = remainder - (double)a1 * 10.0;
	a2 = (int)(remainder);
	remainder = remainder - (double)a2;
	a3 = (int)(24.0 * remainder);
	GridSquare6Digits[0] = (char)o1 + 'A';
	GridSquare6Digits[1] = (char)a1 + 'A';
	GridSquare6Digits[2] = (char)o2 + '0';
	GridSquare6Digits[3] = (char)a2 + '0';
	GridSquare6Digits[4] = (char)o3 + 'A';
	GridSquare6Digits[5] = (char)a3 + 'A';
	GridSquare6Digits[6] = (char)0;
}

//
// Get position altitude andtime fix from GPS
//
// nFixes should be 45 or less.
void GetFix(int nFixes)
{

#if DEBUGMESSAGES
	printf("GetFix()\n");
#endif 

	FILE *fp=fopen("/usr/local/bin/alt","r");
	fscanf(fp, "%f",&CurrAlt);
	fclose(fp);
	printf("A=%.2f,",CurrAlt);
	fp=fopen("/usr/local/bin/lat","r");
	fscanf(fp, "%f",&CurrLat);
	fclose(fp);
	printf("%f,",CurrLat);
	fp=fopen("/usr/local/bin/lon","r");
	fscanf(fp, "%f",&CurrLon);
	// Adjust for West-of-the-Equator
	CurrLon = 0.0 - CurrLon;
	fclose(fp);
	printf("%f,",CurrLon);

	Calc6DigitGridSquare(CurrentGridSquare, CurrLat / 1000000.0, CurrLon / 1000000.0);

#if DEBUGMESSAGES
	//Serial.print("Min=");Serial.println(CurrMinutes);
	printf(",%f,%f,", CurrLat, CurrLon);
	printf(",A=%.2f,",CurrAlt);
	//Serial.print(",H=");Serial.println(gps.hdop());
	printf(",G=%s,",CurrentGridSquare);
#endif

	//CurrFix++; // Bump Fix count

}



// Read the temp DS18B20 sensor
void GetCurrentTemp()
{
	FILE *fp=fopen("/usr/local/bin/temp","r");
	int blah1;
	fscanf(fp,"%f,C,%d,F,%f,C,",&CurrentTemp1,&blah1,&CurrentTemp2);
	fclose(fp);
	//CurrentTemp1 = (float)raw / 16.0;  // Degrees C
}


//
// Computes predicted frequency drift due to temperature changes
// Returns 110th of Hz
//
int CalcFreqDriftDelta(float TempC)
{
	//
	//debug
	//
	//return 0;

	printf("C=%f",TempC);
	float TempCSquared = TempC * TempC;

	// Formula derrived from curve fit from test data. Polynomial curve order of 2;

	//  Formula: -3.132026502�10-3�x2�- 5.491572347 x�+ 136.3524712
	double fy = (-0.003132026502*TempCSquared) - (5.491572347*TempC) + 136.3524712;  //Predicted Freq shift (HZ)
	printf("Calc drift=%f",fy);

	
	int iy = fy * SI5351_FREQ_MULT;  // Convert to 100th Hz
	printf("Drift Delta(hz)=%f",fy);

	return iy;

}


//
//  Power Management etc.
//


//
// Get the radio board set up and ready to transmit
//
void StartRadio()
{

	if (!bRadioIsOn)  // Only start radio if it is off.
	{
		RadioPower(ON);
		sleep(1); // the radio needs a slight "Warm up" time, after power up

		//si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);  //Etherkit Boards
		si5351.init(SI5351_CRYSTAL_LOAD_10PF, 0, 0); // Adafruit boards

													 // Set CLK0 output
		//si5351.set_correction(CALIBRATIONADJUST); // Measured value from calibration program
		si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
		int ret_val = si5351.set_freq(WSPR_DEFAULT_FREQ, SI5351_CLK0); // Get radio on freq. Use WSPR freq for now.
//fprintf(stderr, "\nDEBUG: set_freq(%ld) returned %d\n", WSPR_DEFAULT_FREQ, ret_val);
		si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA); // Set for max power if desired
		si5351.output_enable(SI5351_CLK0, 0); // Disable the clock initially
		printf("si5351 Started (Clock0 off)\n");
	}
	else // Nothing to do, leave radio alone.
	{
		printf("si5351 already started.\n");
	}
	
	
}


//
// Xmit the encoded telemetry symbols
//
void SendTelemetry(unsigned long XmitFrequency, uint8_t *tx_buffer, int symbol_count, uint16_t tone_spacing, uint16_t tone_divisor, unsigned long tone_delay, bool bTuningGuide)
{
	unsigned long millicount = 0L; // DEBUG

	// Start Transmitting.
	printf("\nClock0 on.\n");
	si5351.output_enable(SI5351_CLK0, 1);  // Radio xmit enabled. 

	uint64_t AdjustedXmitFreq = (XmitFrequency * SI5351_FREQ_MULT);  //Convert to 100th Hz
	AdjustedXmitFreq -= CalcFreqDriftDelta(CurrentTemp1); //adjust for temp
//fprintf(stderr, "\nDEBUG: AdjustedXmitFreq: %ld\n",AdjustedXmitFreq);

	// Radio oscillator drifts up in freq as ambient temp cools down. Need time to fine tune signal on Rx.
	if (bTuningGuide) // Xmit tuning guide?
	{

		// Xmit tuning guide tone for FSQ
		printf("\nTune Start.\n");
		int ret_val = si5351.set_freq(AdjustedXmitFreq, SI5351_CLK0);
//fprintf(stderr, "\nDEBUG: set_freq(%ld) returned %d\n", AdjustedXmitFreq, ret_val);
		sleep(7);  // 7 Seconds.
		printf("\nTune Stop.\n");
	}

	printf("Symbols Start...\n");
	// Now transmit the channel symbols
	for (int i = 0; i < symbol_count; i++)
	{
		auto starttime = std::chrono::high_resolution_clock::now();
		//printf("Symbol %d Start...", i);
		int ret_val = si5351.set_freq(AdjustedXmitFreq + ((tx_buffer[i] * tone_spacing)/ tone_divisor), SI5351_CLK0);
//fprintf(stderr, "\nDEBUG: set_freq(%ld) returned %d\n", (AdjustedXmitFreq + ((tx_buffer[i] * tone_spacing)/ tone_divisor)), ret_val);
		auto endtime = std::chrono::high_resolution_clock::now();
		unsigned long duration=0;
		do {
			endtime = std::chrono::high_resolution_clock::now();
			duration = (unsigned long)(std::chrono::duration_cast<std::chrono::microseconds>(endtime - starttime).count());

		} while (duration < tone_delay); // Spin until time is up
		//printf("Symbol %d Stop\n", i);
		
	}
	printf("\nSymbols Stop.\n");

	// Turn off the radio outout
	printf("\nClock0 off.\n");
	si5351.output_enable(SI5351_CLK0, 0);

	//Serial.print("Ave Function Overhead=");Serial.println(millicount / 162.0); //Average time for WSPR.
}

//
//  Encode and send FSQ telemetry.
//
// EXAMPLE: XmitFSQ(FSQ_6_DELAY,false);
//***************************************
void XmitFSQ(unsigned long BaudRate,bool bLeaveRadioOn)
{
	//unsigned long XmitFrequency = FSQ_DEFAULT_FREQ;

	//tone_spacing = FSQ_TONE_SPACING;
	//tone_delay = BaudRate;

	//Serial.print("FSQ ");Serial.println(BaudRate);

	StartRadio();  // Init Radio.
	sleep(1);

	// Clear out the transmit buffer
	memset(tx_buffer, 0, TXBUFFERSIZE);

	// KJ6FO/B FS#2 FFFFF HHH:MM 999999.9m -99.9999,-999.9999 -99.99c 9.99v  // Message format

	// Update Position
	GetFix(1);

	// Update Temp 
	GetCurrentTemp();

	// Compose Telemetry message
	char Message[299+1];


	// Get current time
	time_t theTime = time(NULL);
	struct tm *aTime = localtime(&theTime);

	CurrDays = aTime->tm_mday;
	CurrHours = aTime->tm_hour;
	CurrMinutes = aTime->tm_min;

/*
 */
	sprintf(Message,"[fs2.txt]hab %d %d:%02d:%02d %.2fm %02.6f,%03.6f %3.1fc %3.1fv     \\b",
	    CurrFix,		      // OK
	    CurrDays,			 // OK
	    CurrHours,
	    CurrMinutes,
	    CurrAlt,				      // OK
	    CurrLat,
	    CurrLon,
	    CurrentTemp1,
	    CurrentTemp2);

	// Encode the message into FSQ Symbols
	//uint8_t symbol_count =  jtencode.fsq_encode(MyCallsign, Message.c_str(), tx_buffer);
	uint8_t symbol_count = jtencode.fsq_dir_encode(MyCallsign, "allcall", '#', Message, tx_buffer);
	//Serial.print("bufcount="); Serial.println(symbol_count);
	SendTelemetry(FSQ_DEFAULT_FREQ, tx_buffer, symbol_count,FSQ_TONE_SPACING, FSQ_TONE_DIVISOR, BaudRate, true);

	if (!bLeaveRadioOn)
	{
		RadioPower(OFF);  //Power down Radio
	}
	
}

//
//  Send WSPR reports.
//
void XmitWSPR(int MessageType)
{
	bool bLeaveRadioOn = false;
	StartRadio(); // Init Radio.
	sleep(1); // WSPR starts at 1 sec into minute. + ?a little extra to allow for GPS errors. Dont want to start too early.

	//unsigned long XmitFrequency = WSPR_DEFAULT_FREQ + WSPR_DEFAULT_FREQ_ADJ;
	GetCurrentTemp(); // required for Frequency temp adjustments

	uint8_t symbol_count = WSPR_SYMBOL_COUNT; // From the library defines

	// Clear out the transmit buffer
	memset(tx_buffer, 0, TXBUFFERSIZE);

	// Encode message type
	switch (MessageType)
	{
		// Not using WSPR Type 1 message
		//case WSPRType1:
		//	//Serial.println("WSPR Type 1");
		//	//jtencode.wspr_encode(MyCall, MyGridSquare, RadioPower_dbm, tx_buffer);  // Type 1
		//	break;

	case WSPRType2:
	{
		printf("WSPR Type 2\n");
		jtencode.wspr_encodeType2(MyCallsign, RadioPower_dbm, tx_buffer); // Type 2		
		bLeaveRadioOn = true; // A type 3 will follow so leave radio running and keep warm.
	}
	break;

	case WSPRType3:
	{
		printf("WSPR Type 3\n");
		jtencode.wspr_encodeType3(MyCallsign, CurrentGridSquare, RadioPower_dbm, tx_buffer);  // Type 3
	}
	break;
	}

	SendTelemetry(WSPR_DEFAULT_FREQ + WSPR_DEFAULT_FREQ_ADJ , tx_buffer, symbol_count, WSPR_TONE_SPACING, WSPR_TONE_DIVISOR,WSPR_DELAY, false);

	if (!bLeaveRadioOn)
	{
		RadioPower(OFF);  //Power down Radio
	}
}


int main(int argc, char *argv[])
{
	
	CurrFix = atoi(argv[1]);
	StartRadio();
	XmitFSQ(FSQ_3_DELAY,false);

	return 0;
}

