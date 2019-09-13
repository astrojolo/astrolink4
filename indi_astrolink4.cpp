/*******************************************************************************
 Copyright(c) 2019 astrojolo.com
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
#include "indi_astrolink4.h"

#include "indicom.h"

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <memory>
#include <regex>
#include <cstring>

#define VERSION_MAJOR 0
#define VERSION_MINOR 1

#define TIMERDELAY          3000 // 3s delay for sensors readout
#define MAX_STEPS           10000 // maximum steppers' value
#define ASTROLINK4_LEN      100
#define ASTROLINK4_TIMEOUT  3

// We declare a pointer to IndiAstrolink4
std::unique_ptr<IndiAstrolink4> indiAstrolink4(new IndiAstrolink4());

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
	INDI::DefaultDevice::ISGetProperties(dev);
}
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
	indiAstrolink4->ISNewSwitch(dev, name, states, names, num);
}
void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
	INDI::DefaultDevice::ISNewText (dev, name, texts, names, n);
}
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
	indiAstrolink4->ISNewNumber(dev, name, values, names, num);
}
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int num)
{
    INDI::DefaultDevice::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, num);
}
void ISSnoopDevice (XMLEle *root)
{
    indiAstrolink4->ISSnoopDevice(root);
}

IndiAstrolink4::IndiAstrolink4()
{
	setVersion(VERSION_MAJOR,VERSION_MINOR);
}

bool IndiAstrolink4::Handshake()
{
    // check device
    char res[ASTROLINK4_LEN] = {0};
    if(sendCommand("#", res))
    {
        if(strncmp(res, "#:AstroLink4mini", 15) != 0)
        {
            LOG_ERROR("Device not recognized.");
            return false;
        }
        else
        {
            SetTimer(TIMERDELAY);
            return true;
        }
    }
}

void IndiAstrolink4::TimerHit()
{
	if(isConnected())
	{
		// raw data from serial:
		// q:<stepper position>:<distance to go>:<current>:
		// <sensor 1 type>:<sensor 1 temp>:<sensor 1 humidity>:<dewpoint>:<sensor 2 type>:<sensor 2 temp>:
		//<pwm1>:<pwm2>:<out1>:<out2>:<out3>:
		//<Vin>:<Vreg>:<Ah>:<Wh>:<DCmotorMove>:<CompDiff>:<OverProtectFlag>:<OverProtectValue>

        char res[ASTROLINK4_LEN] = {0};
        if (sendCommand("q", res))
        {
            std::vector<std::string> result = split(res, ":");

            PowerDataN[2].value = std::stod(result[3])
            if(result.size() > 4)
            {
                Sensor1N[0].value = std::stod(result[5]);
                Sensor1N[1].value = std::stod(result[6]);
                Sensor1N[2].value = std::stod(result[7]);
                Sensor1NP.s=IPS_OK;
                IDSetNumber(&Sensor1NP, NULL);
                
                Focus1AbsPosN[0].value = std::stod(result[1]);
                Focus1AbsPosNP.s=IPS_OK;
                IDSetNumber(&Focus1AbsPosNP, NULL);
                
                PWMN[0].value = std::stod(result[10]);
                PWMN[1].value = std::stod(result[11]);
                PWMNP.s=IPS_OK;
                IDSetNumber(&PWMNP, NULL);
                
                PowerDataN[0].value = std::stod(result[15]);
                PowerDataN[1].value = std::stod(result[16])
                PowerDataN[3].value = std::stod(result[17])
                PowerDataN[4].value = std::stod(result[18])
            }
            PowerDataNP.s=IPS_OK;
            IDSetNumber(&PowerDataNP, NULL);
        }

/*
		parsed data:
		sensor[0] = q
		sensor[1] = stepper position
		sensor[2] = stepper steps to go
		sensor[3] = current
		sensor[4] = sensor 1 type
		sensor[5] = sensor 1 temp
		sensor[6] = sensor 1 hum
		sensor[7] = sensor 1 dew point
		sensor[8] = sensor 2 type
		sensor[9] = sensor 2 temp
		sensor[10] = pwm 1
		sensor[11] = pwm 2
		sensor[12] = out 1
		sensor[13] = out 2
		sensor[14] = out 3
		sensor[15] = Vin
		sensor[16] = Vreg
		sensor[17] = Ah
		sensor[18] = Wh
		sensor[19] = DC motor move
		sensor[20] = CompDiff
		sensor[21] = OverprotectFlag
		sensor[22] = OverProtectValue
*/
		SetTimer(TIMERDELAY);
    }
}
const char * IndiAstrolink4::getDefaultName()
{
        return (char *)"AstroLink 4";
}

bool IndiAstrolink4::initProperties()
{
    // We init parent properties first
    INDI::DefaultDevice::initProperties();

	// addDebugControl();
	addSimulationControl();

	// power lines
    IUFillText(&PowerLabelsT[0], "POWER_LABEL_1", "12V out 1", "12V out 1");
    IUFillText(&PowerLabelsT[1], "POWER_LABEL_2", "12V out 2", "12V out 2");
    IUFillText(&PowerLabelsT[2], "POWER_LABEL_3", "12V out 3", "12V out 3");
    IUFillTextVector(&PowerLabelsTP, PowerLabelsT, 3, getDeviceName(), "POWER_CONTROL_LABEL", "12V outputs labels", SETTINGS_TAB, IP_WO, 60, IPS_IDLE);
    
    char portLabel[MAXINDILABEL];
    
    memset(portLabel, 0, MAXINDILABEL);
    int portRC = IUGetConfigText(getDeviceName(), PowerLabelsTP.name, PowerLabelsT[0].name, portLabel, MAXINDILABEL);
    IUFillSwitch(&Power1S[0], "PWR1BTN_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power1S[1], "PWR1BTN_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&Power1SP, Power1S, 2, getDeviceName(), "DC1", portRC == -1 ? "12V out 1" : portLabel, POWER_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    
    memset(portLabel, 0, MAXINDILABEL);
    portRC = IUGetConfigText(getDeviceName(), PowerLabelsTP.name, PowerLabelsT[1].name, portLabel, MAXINDILABEL);
    IUFillSwitch(&Power2S[0], "PWR2BTN_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power2S[1], "PWR2BTN_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&Power2SP, Power2S, 2, getDeviceName(), "DC2", portRC == -1 ? "12V out 2" : portLabel, POWER_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    memset(portLabel, 0, MAXINDILABEL);
    portRC = IUGetConfigText(getDeviceName(), PowerLabelsTP.name, PowerLabelsT[2].name, portLabel, MAXINDILABEL);
    IUFillSwitch(&Power3S[0], "PWR3BTN_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power3S[1], "PWR3BTN_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&Power3SP, Power3S, 2, getDeviceName(), "DC3", portRC == -1 ? "12V out 3" : portLabel, POWER_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

	// focuser
    IUFillSwitch(&Focus1MotionS[0],"FOCUS1_INWARD","Focus In",ISS_OFF);
    IUFillSwitch(&Focus1MotionS[1],"FOCUS1_OUTWARD","Focus Out",ISS_ON);
    IUFillSwitchVector(&Focus1MotionSP, Focus1MotionS, 2, getDeviceName(), "FOCUS1_MOTION", "Direction", FOCUS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    IUFillNumber(&Focus1AbsPosN[0], "FOCUS1_ABSOLUTE_POSITION", "Steps", "%06d", 0, MAX_STEPS, 50, 0);
    IUFillNumberVector(&Focus1AbsPosNP, Focus1AbsPosN, 1, getDeviceName(), "FOCUS1_ABS", "Absolute Position", FOCUS_TAB, IP_RW, 0, IPS_OK);

	// sensors
    IUFillNumber(&Sensor1N[0], "SENSOR1_TEMP", "Temperature [C]", "%4.2f", 0, 100, 0, 0);
    IUFillNumber(&Sensor1N[1], "SENSOR1_HUM", "Humidity [%]", "%4.2f", 0, 100, 0, 0);
    IUFillNumber(&Sensor1N[2], "SENSOR1_DEW", "Dew Point [C]", "%4.2f", 0, 100, 0, 0);
    IUFillNumberVector(&Sensor1NP, Sensor1N, 3, getDeviceName(), "SENSOR1", "DHT sensor", ENVIRONMENT_TAB, IP_RO, 60, IPS_OK);

	// pwm
    IUFillNumber(&PWMN[0], "PWM1_VAL", "A", "%3.0f", 0, 100, 10, 0);
    IUFillNumber(&PWMN[1], "PWM2_VAL", "B", "%3.0f", 0, 100, 10, 0);
    IUFillNumberVector(&PWMNP, PWMN, 2, getDeviceName(), "PWM", "PWM", POWER_TAB, IP_RW, 60, IPS_OK);
    
    IUFillNumber(&PowerDataN[0],"VIN", "Input voltage", "%3.0f", 0, 15, 10, 0);
    IUFillNumber(&PowerDataN[1],"VREG", "Regulated voltage", "%3.0f", 0, 15, 10, 0);
    IUFillNumber(&PowerDataN[2],"ITOT", "Total current", "%3.0f", 0, 15, 10, 0);
    IUFillNumber(&PowerDataN[3],"AH", "Energy consumed [Ah]", "%3.0f", 0, 1000, 10, 0);
    IUFillNumber(&PowerDataN[4],"WH", "Energy consumed [Wh]", "%3.0f", 0, 10000, 10, 0);
    IUFillNumberVector(&PowerDataNP, PowerDataN, 5, getDeviceName(), "POWER_DATA", "Power data", POWER_TAB, IP_RO, 60, IPS_OK);
    
    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]() { return Handshake();});
	registerConnection(serialConnection);

	serialConnection->setDefaultPort("/dev/ttyUSB0");
    serialConnection->setDefaultBaudRate(serialConnection->B_115200);

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
		defineNumber(&Focus1AbsPosNP);
		defineNumber(&Sensor1NP);
		defineNumber(&PWMNP);
        defineNumber(&PowerDataNP);
        defineText(&PowerLabelsTP);
    }
    else
    {
		// We're disconnected
		deleteProperty(Power1SP.name);
		deleteProperty(Power2SP.name);
		deleteProperty(Power3SP.name);
		deleteProperty(Focus1MotionSP.name);
		deleteProperty(Focus1AbsPosNP.name);
		deleteProperty(Sensor1NP.name);
		deleteProperty(PWMNP.name);
        deleteProperty(PowerDataNP.name);
        deleteProperty(PowerLabelsTP.name);
    }

    return true;
}


bool IndiAstrolink4::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	// first we check if it's for our device
    if (!strcmp(dev, getDeviceName()))
    {
        char cmd[ASTROLINK4_LEN] = {0};
        char res[ASTROLINK4_LEN] = {0};
        
		// handle focuser 1 - absolute
		if (!strcmp(name, Focus1AbsPosNP.name))
		{
            sprintf(cmd, "R:0:%d", static_cast<uint8_t>(values[0]));
            bool allOk = sendCommand(cmd, res);
            Focus1AbsPosN.s = allOk ? IPS_OK : IPS_ALERT;
            if(allOk)
                IUUpdateNumber(&Focus1AbsPosNP, values, names, n);
            IDSetNumber(&Focus1AbsPosNP, NULL);

            return true;
		}


		// handle PWM
		if (!strcmp(name, PWMNP.name))
		{
            bool allOk = true;
            if(PWMN[0].value != values[0])
            {
                sprintf(cmd, "B:0:%d", static_cast<uint8_t>(values[0]));
                allOk = allOk && sendCommand(cmd, res);
            }
            if(PWMN[1].value != values[1])
            {
                sprintf(cmd, "B:1:%d", static_cast<uint8_t>(values[1]));
                allOk = allOk && sendCommand(cmd, res);
            }
            PWMN.s = (allOk) ? IPS_OK : IPS_ALERT;
            if(PWMN.s == IPS_OK)
                IUUpdateNumber(&PWMNP, values, names, n);
            IDSetNumber(&PWMNP, NULL);
            return true;
		}
	}
	return INDI::DefaultDevice::ISNewNumber(dev,name,values,names,n);
}


bool IndiAstrolink4::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	// first we check if it's for our device
    if (!strcmp(dev, getDeviceName()))
    {
        char cmd[ASTROLINK4_LEN] = {0};
        char res[ASTROLINK4_LEN] = {0};
        
        // handle power line 1
		if (!strcmp(name, Power1SP.name))
		{
            sprintf(cmd, "C:0:%s", (Power1S[0].s == ISS_ON) ? "1" : "0");
            bool allOk = sendCommand(cmd, res);
            Power1SP.s = allOk ? IPS_OK : IPS_ALERT;
            if(allOk)
                IUUpdateSwitch(&Power1SP, states, names, n);
            
            IDSetSwitch(&Power1SP, NULL);
            return true;
		}
        
        // handle power line 2
        if (!strcmp(name, Power2SP.name))
        {
            sprintf(cmd, "C:1:%s", (Power2S[0].s == ISS_ON) ? "1" : "0");
            bool allOk = sendCommand(cmd, res);
            Power2SP.s = allOk ? IPS_OK : IPS_ALERT;
            if(allOk)
                IUUpdateSwitch(&Power2SP, states, names, n);
            
            IDSetSwitch(&Power2SP, NULL);
            return true;
        }
        
        // handle power line 3
        if (!strcmp(name, Power3SP.name))
        {
            sprintf(cmd, "C:2:%s", (Power3S[0].s == ISS_ON) ? "1" : "0");
            bool allOk = sendCommand(cmd, res);
            Power3SP.s = allOk ? IPS_OK : IPS_ALERT;
            if(allOk)
                IUUpdateSwitch(&Power3SP, states, names, n);
            
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

bool IndiAstrolink4::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Power Labels
        if (!strcmp(name, PowerLabelsTP.name))
        {
            IUUpdateText(&PowerLabelsTP, texts, names, n);
            PowerControlsLabelsTP.s = IPS_OK;
            LOG_INFO("Power port labels saved. Driver must be restarted for the labels to take effect.");
            saveConfig();
            IDSetText(&PowerLabelsTP, nullptr);
            return true;
        }
    }
    
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}


bool IndiAstrolink4::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    
    IUSaveConfigText(fp, &PowerLabelsTP);
    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool IndiAstrolink4::sendCommand(const char * cmd, char * res)
{
    int nbytes_read = 0, nbytes_written = 0, tty_rc = 0;
    char command[ASTROLINK4_LEN];
    char buffer[ASTROLINK4_LEN];

    if(isSimulation())
    {
        if(strcmp(cmd, "#") == 0) sprintf(res, "%s\n", "#:AstroLink4mini");
        if(strcmp(cmd, "q") == 0) sprintf(res, "%s\n", "q:1234:0:1.07:1:2.12:45.1:-12.81:0:0:45:0:0:0:1:12.1:5.0:1.12:13.41:0:34:0:0");
        if(strcmp(cmd, "p") == 0) sprintf(res, "%s\n", "p:1234");
        if(strcmp(cmd, "i") == 0) sprintf(res, "%s\n", "i:0");
        if(strncmp(cmd, "R", 1) == 0) sprintf(res, "%s\n", "R:");
    }
    else
    {
        tcflush(PortFD, TCIOFLUSH);
        sprintf(command, "%s\n", cmd);
        if ( (tty_rc = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
            return false;

        if (!res)
        {
            tcflush(PortFD, TCIOFLUSH);
            return true;
        }

        if ( (tty_rc = tty_nread_section(PortFD, res, ASTROLINK4_LEN, stopChar, ASTROLINK4_TIMEOUT, &nbytes_read)) != TTY_OK || nbytes_read == 1)
            return false;

        tcflush(PortFD, TCIOFLUSH);
        res[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES <%s>", res);

        if (tty_rc != TTY_OK)
        {
            char errorMessage[MAXRBUF];
            tty_error_msg(tty_rc, errorMessage, MAXRBUF);
            LOGF_ERROR("Serial error: %s", errorMessage);
            return false;
        }
    }
    return (cmd[0] == res[0]);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
std::vector<std::string> IndiAstrolink4::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}
