/*******************************************************************************
 Copyright(c) 2019 astrojolo AT astrojolo.com
 .
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <memory>
#include "indi_astrolink4.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1

#define TIMERDELAY 3000 // 3s delay for sensors readout
#define MAX_STEPS 10000 // maximum steppers' value

// We declare a pointer to IndiAstrolink4
std::unique_ptr<IndiAstrolink4> indiAstrolink4(new indiAstrolink4);

void ISPoll(void *p);
void ISInit()
{
   static int isInit = 0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(indiAstrolink4.get() == 0) indiAstrolink4.reset(new IndiAstrolink4());

}
void ISGetProperties(const char *dev)
{
	ISInit();
	indiAstrolink4->ISGetProperties(dev);
}
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
	ISInit();
	indiAstrolink4->ISNewSwitch(dev, name, states, names, num);
}
void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
	ISInit();
	indiAstrolink4->ISNewText(dev, name, texts, names, num);
}
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
	ISInit();
	indiAstrolink4->ISNewNumber(dev, name, values, names, num);
}
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int num)
{
	INDI_UNUSED(dev);
	INDI_UNUSED(name);
	INDI_UNUSED(sizes);
	INDI_UNUSED(blobsizes);
	INDI_UNUSED(blobs);
	INDI_UNUSED(formats);
	INDI_UNUSED(names);
	INDI_UNUSED(num);
}
void ISSnoopDevice (XMLEle *root)
{
    ISInit();
    indiAstrolink4->ISSnoopDevice(root);
}
IndiAstrolink4::IndiAstrolink4()
{
	setVersion(VERSION_MAJOR,VERSION_MINOR);
}
IndiAstrolink4::~IndiAstrolink4()
{
}
bool IndiAstrolink4::Handshake()
{

	if (isSimulation()) {
		IDMessage(getDeviceName(), "Simulation: connected");
		PortFD = 1;
	} else {
		// get port
		PortFD = serialConnection->getPortFD();
		
		// check device
		if(strcmp(serialCom("#"), "#:AstroLink 4 mini") != 0)
		{
			IDMessage(getDeviceName(), "Device not recognized.");
			return false;
		}

		SetTimer(TIMERDELAY);

		IDMessage(getDeviceName(), "AstroLink 4 connected successfully.");
	}

    return true;
}
void IndiAstrolink4::TimerHit()
{
	if(isConnected())
	{
		char* info = serialCom("q");

		// raw data from serial:
		// q:stepper 0 current position : stepper 0 steps to go : stepper 1 current position : stepper 1 steps to go : total current consumption : 
		// if any stepper is moving response ends here – no more values are measured and returned
		// sensor 0 temperature : sensor 0 humidity : sensor 0 dew point : sensor 1 temperature : sensor 1 humidity : sensor 1 dew point : sensor 2 temperature : sensor 2 humidity : sensor 2 dew point : pwm 0 : pwm 1 : pwm 2 : pwm 3 : DC out 0 : DC out 1 : DC out 2 : DC out 3 : input voltage : regulated voltage : 5V voltage : energy consumed in Ah : energy consumed in Wh : DC motor in move :
		// if MLX sensor is not connected response ends here
		// ambient temperature : sky temperature

		char * pch;
		char * sensor[32];

		pch = strtok (info,":");
		int index = 0;
		while (pch != NULL)
		{
			//IDMessage(getDeviceName(), "token %d: %s", index, pch);
			sensor[index] = pch;
			pch = strtok (NULL, ":");
			index++;
		}

/*
		parsed data:
		sensor[0] = q
		sensor[1] = stepper 0 current position
		sensor[2] = stepper 0 steps to go
		sensor[3] = stepper 1 current position
		sensor[4] = stepper 1 steps to go
		sensor[5] = total current consumption
		sensor[6] = sensor 0 temperature
		sensor[7] = sensor 0 humidity
		sensor[8] = sensor 0 dew point
		sensor[9] = sensor 1 temperature
		sensor[10] = sensor 1 humidity
		sensor[11] = sensor 1 dew point
		sensor[12] = sensor 2 temperature
		sensor[13] = sensor 2 humidity
		sensor[14] = sensor 2 dew point
		sensor[15] = pwm 0
		sensor[16] = pwm 1
		sensor[17] = pwm 2
		sensor[18] = pwm 3
		sensor[19] = DC out 0
		sensor[20] = DC out 1
		sensor[21] = DC out 2
		sensor[22] = DC out 3
		sensor[23] = input voltage
		sensor[24] = regulated voltage
		sensor[25] = 5V voltage
		sensor[26] = energy consumed in Ah
		sensor[27] = energy consumed in Wh
		sensor[28] = DC motor in move
		sensor[29] = ambient temperature
		sensor[30] = sky temperature
		sensor[31] = 0

*/

		if (!strcmp(sensor[0], "q"))
		{
			//IDMessage(getDeviceName(), "Sensors data received");

			Sensor1NP.s=IPS_BUSY;
			IDSetNumber(&Sensor1NP, NULL);
			Sensor1N[0].value = atof(sensor[6]);
			Sensor1N[1].value = atof(sensor[7]);
			Sensor1N[2].value = atof(sensor[8]);
			Sensor1NP.s=IPS_OK;
			IDSetNumber(&Sensor1NP, NULL);


			// update values of various controls
			Focus1AbsPosN[0].value = atof(sensor[1]);
			IDSetNumber(&Focus1AbsPosNP, NULL);
			PWM1N[0].value = atof(sensor[18]);
			IDSetNumber(&PWM1NP, NULL);
			PWM2N[0].value = atof(sensor[19]);
			IDSetNumber(&PWM2NP, NULL);
		}

		SetTimer(TIMERDELAY);
    }
}
const char * IndiAstrolink4::getDefaultName()
{
        return (char *)"AStroLink 4";
}
bool IndiAstrolink4::initProperties()
{
    // We init parent properties first
    INDI::DefaultDevice::initProperties();

	// addDebugControl();
	addSimulationControl();

	// power lines
    IUFillSwitch(&Power1S[0], "PWR1BTN_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power1S[1], "PWR1BTN_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&Power1SP, Power1S, 2, getDeviceName(), "DC1", "12V out 1", "12V Outputs", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&Power2S[0], "PWR2BTN_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power2S[1], "PWR2BTN_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&Power2SP, Power2S, 2, getDeviceName(), "DC2", "12V out 2", "12V Outputs", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&Power3S[0], "PWR3BTN_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power3S[1], "PWR3BTN_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&Power3SP, Power3S, 2, getDeviceName(), "DC3", "12V out 3", "12V Outputs", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

	// focuser 1
    IUFillSwitch(&Focus1MotionS[0],"FOCUS1_INWARD","Focus In",ISS_OFF);
    IUFillSwitch(&Focus1MotionS[1],"FOCUS1_OUTWARD","Focus Out",ISS_ON);
    IUFillSwitchVector(&Focus1MotionSP,Focus1MotionS,2,getDeviceName(),"FOCUS1_MOTION","Direction","Focuser",IP_RW,ISR_ATMOST1,60,IPS_OK);

    IUFillNumber(&Focus1StepN[0],"FOCUS1_STEP","Steps","%0.0f",0,(int)MAX_STEPS/10,(int)MAX_STEPS/100,(int)MAX_STEPS/100);
    IUFillNumberVector(&Focus1StepNP,Focus1StepN,1,getDeviceName(),"FOCUS1_STEPSIZE","Step Size","Focuser",IP_RW,60,IPS_OK);

    IUFillNumber(&Focus1AbsPosN[0],"FOCUS1_ABSOLUTE_POSITION","Steps","%0.0f",0,MAX_STEPS,(int)MAX_STEPS/100,0);
    IUFillNumberVector(&Focus1AbsPosNP,Focus1AbsPosN,1,getDeviceName(),"FOCUS1_ABS","Absolute Position","Focuser",IP_RW,0,IPS_OK);

	// sensors
    IUFillNumber(&Sensor1N[0],"SENSOR1_TEMP","Temperature [C]","%0.0f",0,100,0,0);
    IUFillNumber(&Sensor1N[1],"SENSOR1_HUM","Humidity [%]","%0.0f",0,100,0,0);
    IUFillNumber(&Sensor1N[2],"SENSOR1_DEW","Dew Point [C]","%0.0f",0,100,0,0);
    IUFillNumberVector(&Sensor1NP,Sensor1N,3,getDeviceName(),"SENSOR1","Sensor 1","Sensors",IP_RO,60,IPS_OK);	

	// pwm
    IUFillNumber(&PWM1N[0],"PWM1_VAL","Value","%0.0f",0,100,10,0);
    IUFillNumberVector(&PWM1NP,PWM1N,1,getDeviceName(),"PWM1","PWM 1","PWM",IP_RW,60,IPS_OK);

    IUFillNumber(&PWM2N[0],"PWM2_VAL","Value","%0.0f",0,100,10,0);
    IUFillNumberVector(&PWM2NP,PWM2N,1,getDeviceName(),"PWM2","PWM 2","PWM",IP_RW,60,IPS_OK);

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]() { return Handshake();});
	registerConnection(serialConnection);

	serialConnection->setDefaultPort("/dev/ttyUSB0");
//	serialConnection->setDefaultBaudRate(B_115200);

    return true;
}
bool IndiAstrolink4::updateProperties()
{
    // Call parent update properties first
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
		defineSwitch(&Power1SP);
		defineSwitch(&Power2SP);
		defineSwitch(&Power3SP);
		defineSwitch(&Focus1MotionSP);
		defineNumber(&Focus1StepNP);
		defineNumber(&Focus1AbsPosNP);
		defineNumber(&Sensor1NP);
		defineNumber(&PWM1NP);
		defineNumber(&PWM2NP);
  }
    else
    {
		// We're disconnected
		deleteProperty(Power1SP.name);
		deleteProperty(Power2SP.name);
		deleteProperty(Power3SP.name);
		deleteProperty(Focus1MotionSP.name);
		deleteProperty(Focus1StepNP.name);
		deleteProperty(Focus1AbsPosNP.name);
		deleteProperty(Sensor1NP.name);
		deleteProperty(PWM1NP.name);
		deleteProperty(PWM2NP.name);
    }

    return true;
}
void IndiAstrolink4::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    /* Add debug controls so we may debug driver if necessary */
    // addDebugControl();
}
bool IndiAstrolink4::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	// first we check if it's for our device
    if (!strcmp(dev, getDeviceName()))
    {
		// handle focuser 1 - absolute
		if (!strcmp(name, Focus1AbsPosNP.name))
		{
			IUUpdateNumber(&Focus1AbsPosNP,values,names,n);

			char stepval[8];
			char newval[8];
			sprintf(stepval, "R:0:%0.0f", Focus1AbsPosN[0].value);
			serialCom(stepval);
			sprintf(newval, "p:%0.0f", Focus1AbsPosN[0].value);

			// loop until new position is reached
			while ( strcmp(serialCom("p:0"), newval) )
			{
				IDMessage(getDeviceName(), "Focuser moving to the position...");
				usleep(100 * 1000);
			}

			// if reached new position update client
			if (!strcmp(serialCom("p:0"),newval))
			{
				IDMessage(getDeviceName(), "Focuser at the position %0.0f", Focus1AbsPosN[0].value);
				IDSetNumber(&Focus1AbsPosNP, NULL);
				return true;
			} else {
				return false;
			}
		}

		// handle focuser 1 - relative
		if (!strcmp(name, Focus1StepNP.name))
		{
			IUUpdateNumber(&Focus1StepNP,values,names,n);
			IDSetNumber(&Focus1StepNP, NULL);

			if (Focus1MotionS[0].s == ISS_ON)
			{
				if ( Focus1AbsPosN[0].value - Focus1StepN[0].value >= Focus1AbsPosN[0].min )
					Focus1AbsPosN[0].value -= Focus1StepN[0].value;
			} else {
				if ( Focus1AbsPosN[0].value + Focus1StepN[0].value <= Focus1AbsPosN[0].max )
					Focus1AbsPosN[0].value += Focus1StepN[0].value;
			}

			char stepval[8];
			char newval[8];
			sprintf(stepval, "R:0:%0.0f", Focus1AbsPosN[0].value);
			serialCom(stepval);
			sprintf(newval, "p:%0.0f", Focus1AbsPosN[0].value);

			// loop until new position is reached
			while ( strcmp(serialCom("p:0"), newval) )
			{
				IDMessage(getDeviceName(), "Focuser moving to the position...");
				usleep(100 * 1000);
			}

			// if reached new position update client
			if (!strcmp(serialCom("p:0"),newval))
			{
				IDMessage(getDeviceName(), "Focuser at the position %0.0f", Focus1AbsPosN[0].value);
				IDSetNumber(&Focus1AbsPosNP, NULL);
				return true;
			} else {
				return false;
			}
		}

		// handle PWM1
		if (!strcmp(name, PWM1NP.name))
		{
			IUUpdateNumber(&PWM1NP,values,names,n);
			char pwmval[8];
			char newval[8];
			sprintf(pwmval, "B:0:%0.0f", PWM1N[0].value);
			serialCom(pwmval);
			sprintf(newval, "b:%0.0f", PWM1N[0].value);
			if (!strcmp(serialCom("b:0"),newval))
			{
				PWM1NP.s=IPS_OK;
				IDSetNumber(&PWM1NP, NULL);
				return true;
			} else {
				return false;
			}
		}

		// handle PWM2
		if (!strcmp(name, PWM2NP.name))
		{
			IUUpdateNumber(&PWM2NP,values,names,n);
			char pwmval[8];
			char newval[8];
			sprintf(pwmval, "B:1:%0.0f", PWM2N[0].value);
			serialCom(pwmval);
			sprintf(newval, "b:%0.0f", PWM2N[0].value);
			if (!strcmp(serialCom("b:1"),newval))
			{
				PWM2NP.s=IPS_OK;
				IDSetNumber(&PWM2NP, NULL);
				return true;
			} else {
				return false;
			}
		}

	}
	return INDI::DefaultDevice::ISNewNumber(dev,name,values,names,n);
}
bool IndiAstrolink4::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	// first we check if it's for our device
    if (!strcmp(dev, getDeviceName()))
    {
		// handle power line 1
		if (!strcmp(name, Power1SP.name))
		{
			IUUpdateSwitch(&Power1SP, states, names, n);

			// ON
			if ( Power1S[0].s == ISS_ON )
			{
				if (strcmp(serialCom("C:0:1"),"C:") || strcmp(serialCom("c:0"),"c:1"))
					return false;
			}

			// OFF
			if ( Power1S[1].s == ISS_ON )
			{
				if (strcmp(serialCom("C:0:0"),"C:") || strcmp(serialCom("c:0"),"c:0"))
					return false;
			}

			IDMessage(getDeviceName(), "12V Out 1 is %s", Power1S[0].s == ISS_ON ? "ON" : "OFF" );
			Power1SP.s = IPS_OK;
			IDSetSwitch(&Power1SP, NULL);
			return true;
		}

		// handle power line 2
		if (!strcmp(name, Power2SP.name))
		{
			IUUpdateSwitch(&Power2SP, states, names, n);

			// ON
			if ( Power2S[0].s == ISS_ON )
			{
				if (strcmp(serialCom("C:1:1"),"C:") || strcmp(serialCom("c:1"),"c:1"))
					return false;
			}

			// OFF
			if ( Power2S[1].s == ISS_ON )
			{
				if (strcmp(serialCom("C:1:0"),"C:") || strcmp(serialCom("c:1"),"c:0"))
					return false;
			}

			IDMessage(getDeviceName(), "12V Out 2 is %s", Power2S[0].s == ISS_ON ? "ON" : "OFF" );
			Power2SP.s = IPS_OK;
			IDSetSwitch(&Power2SP, NULL);
			return true;
		}

		// handle power line 3
		if (!strcmp(name, Power3SP.name))
		{
			IUUpdateSwitch(&Power3SP, states, names, n);

			// ON
			if ( Power3S[0].s == ISS_ON )
			{
				if (strcmp(serialCom("C:2:1"),"C:") || strcmp(serialCom("c:2"),"c:1"))
					return false;
			}

			// OFF
			if ( Power3S[1].s == ISS_ON )
			{
				if (strcmp(serialCom("C:2:0"),"C:") || strcmp(serialCom("c:2"),"c:0"))
					return false;
			}

			IDMessage(getDeviceName(), "12V Out 3 is %s", Power3S[0].s == ISS_ON ? "ON" : "OFF" );
			Power3SP.s = IPS_OK;
			IDSetSwitch(&Power3SP, NULL);
			return true;
		}


		// handle focuser 1
		if (!strcmp(name, Focus1MotionSP.name))
		{
			IUUpdateSwitch(&Focus1MotionSP, states, names, n);
			IDSetSwitch(&Focus1MotionSP, NULL);
			return true;
		}

	}
	return INDI::DefaultDevice::ISNewSwitch (dev, name, states, names, n);
}
bool IndiAstrolink4::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	return INDI::DefaultDevice::ISNewText (dev, name, texts, names, n);
}
bool IndiAstrolink4::ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
	return INDI::DefaultDevice::ISNewBLOB (dev, name, sizes, blobsizes, blobs, formats, names, n);
}
bool IndiAstrolink4::ISSnoopDevice(XMLEle *root)
{
    return INDI::DefaultDevice::ISSnoopDevice(root);
}
bool IndiAstrolink4::saveConfigItems(FILE *fp)
{
	IUSaveConfigSwitch(fp, &Power1SP);
	IUSaveConfigSwitch(fp, &Power2SP);
	IUSaveConfigSwitch(fp, &Power3SP);
	IUSaveConfigNumber(fp, &Focus1AbsPosNP);
	IUSaveConfigNumber(fp, &PWM1NP);
	IUSaveConfigNumber(fp, &PWM2NP);
    return true;
}
char* IndiAstrolink4::serialCom(const char* input)
{
	char command[255];
	char buffer[255];
	char* output = new char[255];

	// format input
	sprintf(command, "%s\n", input);

	// write command
	write(PortFD, command, strlen(command));

	// delay
	usleep(200 * 1000);

	// read response
	int res = read(PortFD, buffer, 255);
	buffer[res] = 0;

	// remove line feed
	buffer[strcspn(buffer, "\n\r")] = 0;

	// format output 
	sprintf(output, "%s", buffer);

	// debug response
	// IDLog("AstroLink4 Input: [%s] (length: %d), AstroLink4 Output: [%s] (length: %d)\n", input, (int)strlen(input), output, (int)strlen(output));

	return output;
}
