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
#define VERSION_MINOR 6

#define ASTROLINK4_POLL     2000 // delay for sensors readout
#define ASTROLINK4_LEN      100
#define ASTROLINK4_TIMEOUT  3

//////////////////////////////////////////////////////////////////////
/// Delegates
//////////////////////////////////////////////////////////////////////
std::unique_ptr<IndiAstrolink4> indiAstrolink4(new IndiAstrolink4());

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
    indiAstrolink4->ISGetProperties(dev);
}
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
	indiAstrolink4->ISNewSwitch(dev, name, states, names, num);
}
void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
    indiAstrolink4->ISNewText (dev, name, texts, names, num);
}
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
	indiAstrolink4->ISNewNumber(dev, name, values, names, num);
}
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int num)
{
    indiAstrolink4->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, num);
}
void ISSnoopDevice (XMLEle *root)
{
    indiAstrolink4->ISSnoopDevice(root);
}

//////////////////////////////////////////////////////////////////////
///Constructor
//////////////////////////////////////////////////////////////////////
IndiAstrolink4::IndiAstrolink4() : FI(this), WI(this)
{
	setVersion(VERSION_MAJOR,VERSION_MINOR);
}

const char * IndiAstrolink4::getDefaultName()
{
    return (char *)"AstroLink 4";
}

//////////////////////////////////////////////////////////////////////
/// Communication
//////////////////////////////////////////////////////////////////////
bool IndiAstrolink4::Handshake()
{
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
            SetTimer(ASTROLINK4_POLL);
            return true;
        }
    }
}

void IndiAstrolink4::TimerHit()
{
	if(isConnected())
	{
        sensorRead();
		SetTimer(ASTROLINK4_POLL);
    }
}

//////////////////////////////////////////////////////////////////////
/// Overrides
//////////////////////////////////////////////////////////////////////
bool IndiAstrolink4::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | FOCUSER_INTERFACE | WEATHER_INTERFACE);

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_REVERSE  |
                      FOCUSER_CAN_SYNC     |
                      FOCUSER_CAN_ABORT    |
                      FOCUSER_HAS_BACKLASH);

    FI::initProperties(FOCUS_TAB);
    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);

    addAuxControls();

	// addDebugControl();
	addSimulationControl();

	// power lines
    IUFillText(&PowerLabelsT[0], "POWER_LABEL_1", "12V out 1", "12V out 1");
    IUFillText(&PowerLabelsT[1], "POWER_LABEL_2", "12V out 2", "12V out 2");
    IUFillText(&PowerLabelsT[2], "POWER_LABEL_3", "12V out 3", "12V out 3");
    IUFillTextVector(&PowerLabelsTP, PowerLabelsT, 3, getDeviceName(), "POWER_CONTROL_LABEL", "12V outputs labels", SETTINGS_TAB, IP_WO, 60, IPS_IDLE);

    // focuser settings
    IUFillNumber(&FocuserSettingsN[FS_MAX_POS], "FS_MAX_POS", "Max. position", "%.0f", 0, 1000000, 1000, 10000);
    IUFillNumber(&FocuserSettingsN[FS_SPEED], "FS_SPEED", "Speed [pps]", "%.0f", 0, 4000, 50, 250);
    IUFillNumber(&FocuserSettingsN[FS_STEP_SIZE], "FS_STEP_SIZE", "Step size [um]", "%.2f", 0, 100, 0.1, 5.0);
    IUFillNumber(&FocuserSettingsN[FS_COMPENSATION], "FS_COMPENSATION", "Compensation [steps/C]", "%.1f", -1000, 1000, 1, 0);
    IUFillNumberVector(&FocuserSettingsNP, FocuserSettingsN, 4, getDeviceName(), "FOCUSER_SETTINGS", "Focuser settings", SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    IUFillSwitch(&FocuserModeS[FS_MODE_UNI], "FS_MODE_UNI", "Unipolar", ISS_ON);
    IUFillSwitch(&FocuserModeS[FS_MODE_BI], "FS_MODE_BI", "Bipolar", ISS_OFF);
    IUFillSwitch(&FocuserModeS[FS_MODE_MICRO], "FS_MODE_MICRO", "Microstep", ISS_OFF);
    IUFillSwitchVector(&FocuserModeSP, FocuserModeS, 3, getDeviceName(), "FOCUSER_MODE", "Focuser mode", SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // other settings
    IUFillNumber(&OtherSettingsN[SET_AREF_COEFF], "SET_AREF_COEFF", "V ref coefficient", "%.3f", 0.9, 1.2, 0.001, 1.09);
    IUFillNumber(&OtherSettingsN[SET_OVER_TIME], "SET_OVER_TIME", "Protection sensitivity [ms]", "%.0f", 10, 500, 10, 100);
    IUFillNumber(&OtherSettingsN[SET_OVER_VOLT], "SET_OVER_VOLT", "Protection voltage [V]", "%.1f", 10, 14, 0.1, 14);
    IUFillNumber(&OtherSettingsN[SET_OVER_AMP], "SET_OVER_AMP", "Protection current [A]", "%.1f", 1, 10, 0.1, 10);
    IUFillNumberVector(&OhterSettingsNP, OtherSettingsN, 4, getDeviceName, "OTHER_SETTINGS", "Device settings", SETTINGS_TAB, IP_RW, 60, IPS_IDLE);
    
    // focuser compensation
    IUFillNumber(&CompensationValueN[0], "COMP_VALUE", "Compensation steps", "%.0f", 0, 10000, 1, 0);
    IUFillNumberVector(&CompensationValueNP, CompensationValueN, 1, getDeviceName(), "COMP_STEPS", "Compensation steps", FOCUS_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&CompensateNowS[0], "COMP_NOW", "Compensate now", ISS_OFF);
    IUFillSwitchVector(&CompensateNowSP, CompensateNowS, 1, getDeviceName(), "COMP_NOW", "Compensate now", FOCUS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    
    // power lines
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
    
    IUFillSwitch(&PowerDefaultOnS[0], "POW_DEF_ON1", "DC1", ISS_OFF);
    IUFillSwitch(&PowerDefaultOnS[1], "POW_DEF_ON2", "DC2", ISS_OFF);
    IUFillSwitch(&PowerDefaultOnS[2], "POW_DEF_ON3", "DC3", ISS_OFF);
    IUFillSwitchVector(&PowerDefaultOnSP, PowerDefaultOnS, 3, getDeviceName, "POW_DEF_ON", "Power default ON", SETTINGS_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

	// pwm
    IUFillNumber(&PWMN[0], "PWM1_VAL", "A", "%3.0f", 0, 100, 10, 0);
    IUFillNumber(&PWMN[1], "PWM2_VAL", "B", "%3.0f", 0, 100, 10, 0);
    IUFillNumberVector(&PWMNP, PWMN, 2, getDeviceName(), "PWM", "PWM", POWER_TAB, IP_RW, 60, IPS_IDLE);

    // Auto pwm
    IUFillSwitch(&AutoPWMS[0], "PWMA_A", "A", ISS_OFF);
    IUFillSwitch(&AutoPWMS[1], "PWMA_B", "B", ISS_OFF);
    IUFillSwitchVector(&AutoPWMSP, AutoPWMS, 2, getDeviceName(), "AUTO_PWM", "Auto PWM", POWER_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);
    
    IUFillNumber(&PowerDataN[0],"VIN", "Input voltage", "%.1f", 0, 15, 10, 0);
    IUFillNumber(&PowerDataN[1],"VREG", "Regulated voltage", "%.1f", 0, 15, 10, 0);
    IUFillNumber(&PowerDataN[2],"ITOT", "Total current", "%.1f", 0, 15, 10, 0);
    IUFillNumber(&PowerDataN[3],"AH", "Energy consumed [Ah]", "%.1f", 0, 1000, 10, 0);
    IUFillNumber(&PowerDataN[4],"WH", "Energy consumed [Wh]", "%.1f", 0, 10000, 10, 0);
    IUFillNumberVector(&PowerDataNP, PowerDataN, 5, getDeviceName(), "POWER_DATA", "Power data", POWER_TAB, IP_RO, 60, IPS_IDLE);
    
    // Environment Group
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -15, 35, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 15);
    
    // Sensor 2
    IUFillNumber(&Sensor2N[0], "TEMP_2", "Temperature (C)", "%.1f", -50, 100, 1, 0);
    IUFillNumberVector(&Sensor2NP, Sensor2N, 1, getDeviceName(), "SENSOR_2", "Sensor 2", ENVIRONMENT_TAB, IP_RO, 60, IPS_IDLE);
    
    // DC focuser
    IUFillNumber(&DCFocTimeN[0], "DC_FOC_TIME", "Time [ms]", "%.0f", 1, 5000, 100, 100);
    IUFillNumber(&DCFocTimeN[1], "DC_FOC_PWM", "PWM [%]", "%.0f", 1, 100, 10, 50);
    IUFillNumberVector(&DCFocTimeNP, DCFocTimeN, 1, getDeviceName(), "DC_FOC_TIME", "DC Focuser", FOCUS_TAB, IP_RW, 60, IPS_OK);
    
    IUFillSwitch(&DCFocDirS[0], "DIR_IN", "IN", ISS_OFF);
    IUFillSwitch(&DCFocDirS[1], "DIR_OUT", "OUT", ISS_ON);
    IUFillSwitchVector(&DCFocDirSP, DCFocDirS, 2, getDeviceName(), "DC_FOC_DIR", "DC Focuser direction", FOCUS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
    
    IUFillSwitch(&DCFocStartS[0], "DC_FOC_START", "MOVE", ISS_OFF);
    IUFillSwitchVector(&DCFocStartSP, DCFocStartS, 1, getDeviceName(), "DC_FOC_START", "DC Focuser move", FOCUS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);
    
    IUFillSwitch(&DCFocAbortS[0], "DC_FOC_ABORT", "STOP", ISS_OFF);
    IUFillSwitchVector(&DCFocAbortSP, DCFocAbortS, 1, getDeviceName(), "DC_FOC_ABORT", "DC Focuser stop", FOCUS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

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
        defineSwitch(&AutoPWMSP);
		defineNumber(&Sensor2NP);
		defineNumber(&PWMNP);
        defineNumber(&PowerDataNP);
        defineNumber(&FocuserSettingsNP);
        defineSwitch(&FocuserModeSP);
        defineNumber(&CompensationValueNP);
        defineSwitch(&CompensateNowSP);
        defineSwitch(&PowerDefaultOnSP);
        defineNumber(&OtherSettingsNP);
        defineNumber(&DCFocTimeNP);
        defineSwitch(&DCFocDirSP);
        defineSwitch(&DCFocAbortSP);
        defineSwitch(&DCFocStartSP);
        defineText(&PowerLabelsTP);
        FI::updateProperties();
        WI::updateProperties();
    }
    else
    {
		deleteProperty(Power1SP.name);
		deleteProperty(Power2SP.name);
		deleteProperty(Power3SP.name);
        deleteProperty(AutoPWMSP.name);
		deleteProperty(Sensor2NP.name);
		deleteProperty(PWMNP.name);
        deleteProperty(PowerDataNP.name);
        deleteProperty(PowerLabelsTP.name);
        deleteProperty(FocuserSettingsNP.name);
        deleteProperty(FocuserModeSP.name);
        deleteProperty(CompensateNowSP.name);
        deleteProperty(CompensationValueNP.name);
        deleteProperty(PowerDefaultOnSP.name);
        deleteProperty(OtherSettingsNP.name);
        deleteProperty(DCFocTimeNP.name);
        deleteProperty(DCFocDirSP.name);
        deleteProperty(DCFocAbortSP.name);
        deleteProperty(DCFocStartSP.name);
        FI::updateProperties();
        WI::updateProperties();
    }

    return true;
}


bool IndiAstrolink4::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        char cmd[ASTROLINK4_LEN] = {0};
        char res[ASTROLINK4_LEN] = {0};
        
		// handle PWM
		if (!strcmp(name, PWMNP.name))
		{
            bool allOk = true;
            if(PWMN[0].value != values[0])
            {
                sprintf(cmd, "B:0:%d", static_cast<uint8_t>(values[0]));
                AutoPWMS[0].s = ISS_OFF;
                allOk = allOk && sendCommand(cmd, res);
            }
            if(PWMN[1].value != values[1])
            {
                sprintf(cmd, "B:1:%d", static_cast<uint8_t>(values[1]));
                AutoPWMS[1].s = ISS_OFF;
                allOk = allOk && sendCommand(cmd, res);
            }
            PWMNP.s = (allOk) ? IPS_BUSY : IPS_ALERT;
            if(allOk)
                IUUpdateNumber(&PWMNP, values, names, n);
            IDSetNumber(&PWMNP, NULL);
            IDSetSwitch(&AutoPWMSP, NULL);
            return true;
		}
        
        // Focuser settings
        if(!strcmp(name, FocuserSettingsNP .name))
        {
            snprintf(cmd, ASTROLINK4_LEN, "%s", "u");
            if(sendCommand(cmd, res))
            {
                std::string concatSettings = "U";
                std::vector<std::string> result = split(res, ":");
                if(result.size() >= 10)
                {
                    result[1] = doubleToStr(values[0]);
                    result[2] = doubleToStr(values[1]);
                    result[9] = doubleToStr(values[2] * 100);
                    for (const auto &piece : result) concatSettings += ":" + piece;
                    snprintf(cmd, ASTROLINK4_LEN, "%s", concatSettings.c_str());
                    LOG_ERROR(concatSettings.c_str());
                    if(sendCommand(cmd, res))
                    {
                        FocuserSettingsNP.s = IPS_BUSY;
                        IUUpdateNumber(&FocuserSettingsNP, states, names, n);
                        IDSetNumber(&FocuserSettingsNP, NULL);
                    }
                }
            }
            FocuserSettingsNP.s = IPS_ALERT;
            return true;
        }
        
        // Other settings
        if(!strcmp(name, OtherSettingsNP .name))
        {
            snprintf(cmd, ASTROLINK4_LEN, "%s", "u");
            if(sendCommand(cmd, res))
            {
                std::string concatSettings = "U";
                std::vector<std::string> result = split(res, ":");
                if(result.size() >= 14)
                {
                    result[10] = doubleToStr(values[0]);
                    result[11] = doubleToStr(values[1]);
                    result[12] = doubleToStr(values[2]);
                    result[13] = doubleToStr(values[3]);
                    for (const auto &piece : result) concatSettings += ":" + piece;
                    snprintf(cmd, ASTROLINK4_LEN, "%s", concatSettings.c_str());
                    LOG_ERROR(concatSettings.c_str());
                    if(sendCommand(cmd, res))
                    {
                        OtherSettingsNP.s = IPS_BUSY;
                        IUUpdateNumber(&OtherSettingsNP, values, names, n);
                        IDSetNumber(&OtherSettingsNP, NULL);
                    }
                }
            }
            OtherSettingsNP.s = IPS_ALERT;
            return true;
        }

        // DC Focuser
        if(!strcmp(name, DCFocTimeNP.name))
        {
            IUUpdateNumber(&DCFocTimeNP, values, names, n);
            IDSetNumber(&DCFocTimeNP, NULL);
            DCFocTimeNP.s = IPS_OK;
            return true;
        }
        
        if (strstr(name, "FOCUS_"))
            return FI::processNumber(dev, name, values, names, n);
        if (strstr(name, "WEATHER_"))
            return WI::processNumber(dev, name, values, names, n);
    }

	return INDI::DefaultDevice::ISNewNumber(dev,name,values,names,n);
}


bool IndiAstrolink4::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        char cmd[ASTROLINK4_LEN] = {0};
        char res[ASTROLINK4_LEN] = {0};
        
        // handle power line 1
		if (!strcmp(name, Power1SP.name))
		{
            sprintf(cmd, "C:0:%s", (Power1S[0].s == ISS_ON) ? "1" : "0");
            bool allOk = sendCommand(cmd, res);
            Power1SP.s = allOk ? IPS_BUSY : IPS_ALERT;
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
            Power2SP.s = allOk ? IPS_BUSY : IPS_ALERT;
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
            Power3SP.s = allOk ? IPS_BUSY : IPS_ALERT;
            if(allOk)
                IUUpdateSwitch(&Power3SP, states, names, n);
            
            IDSetSwitch(&Power3SP, NULL);
            return true;
        }

        // compensate now
        if(!strcmp(name, CompensateNowSP.name))
        {
            sprintf(cmd, "S:%d", static_cast<uint8_t>(CompensationValueN[0].value));
            bool allOk = sendCommand(cmd, res);
            CompensateNowSP.s = allOk ? IPS_BUSY : IPS_ALERT;
            if(allOk)
                IUUpdateSwitch(&CompensateNowSP, states, names, n);

            IDSetSwitch(&CompensateNowSP, NULL);
            return true;
        }

        // Auto PWM
        if (!strcmp(name, AutoPWMSP.name))
        {
            ISState pwmA = AutoPWMS[0].s;
            ISState pwmB = AutoPWMS[1].s;
            IUUpdateSwitch(&AutoPWMSP, states, names, n);
            if (setAutoPWM())
            {
                AutoPWMSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&AutoPWMSP);
                AutoPWMS[0].s = pwmA;
                AutoPWMS[1].s = pwmB;
                AutoPWMSP.s = IPS_ALERT;
            }
            IDSetSwitch(&AutoPWMSP, nullptr);
            return true;
        }
        
        // DC Focuser
        if (!strcmp(name, DCFocDirSP.name))
        {
            DCFocDirSP.s = IPS_OK;
            IUUpdateSwitch(&DCFocDirSP, states, names, n);
            IDSetSwitch(&DCFocDirSP, nullptr);
            return true;
        }
        
        if (!strcmp(name, DCFocStartSP.name))
        {
            sprintf(cmd, ASTROLINK4_LEN, "G:%.0f:%.0f:%.0f", DCFocTimeN[0].value, DCFocTimeN[1].value, (DCFocDirS[0].s == ISS_ON) ? 1 : 0);
            if(sendCommand(cmd, res))
            {
                DCFocStartSP.s = IPS_BUSY;
                DCFocAbortSP.s = IPS_OK;
                IUUpdateSwitch(&DCFocStartSP, states, names, n);
                IDSetSwitch(&DCFocStartSP, nullptr);
            }
            DCFocStartSP.s = IPS_ALERT;
            return true;
        }
        
        if (!strcmp(name, DCFocAbortSP.name))
        {
            sprintf(cmd, ASTROLINK4_LEN, "%s", "K");
            if(sendCommand(cmd, res))
            {
                DCFocAbortSP.s = IPS_BUSY;
                IUUpdateSwitch(&DCFocAbortSP, states, names, n);
                IDSetSwitch(&DCFocAbortSP, nullptr);
            }
            DCFocAbortSP.s = IPS_ALERT;
            return true;
        }
        
        // Power default on
        if(!strcmp(name, PowerDefaultOnSP.name))
        {
            snprintf(cmd, ASTROLINK4_LEN, "%s", "u");
            if(sendCommand(cmd, res))
            {
                std::string concatSettings = "U";
                std::vector<std::string> result = split(res, ":");
                if(result.size() >= 15)
                {
                    result[13] = (states[0] == ISS_ON) ? "1" : "0";
                    result[14] = (states[1] == ISS_ON) ? "1" : "0";
                    result[15] = (states[2] == ISS_ON) ? "1" : "0";
                    for (const auto &piece : result) concatSettings += ":" + piece;
                    snprintf(cmd, ASTROLINK4_LEN, "%s", concatSettings.c_str());
                    LOG_ERROR(concatSettings.c_str());
                    if(sendCommand(cmd, res))
                    {
                        PowerDefaultOnSP.s = IPS_BUSY;
                        IUUpdateSwitch(&PowerDefaultOnSP, states, names, n);
                        IDSetSwitch(&PowerDefaultOnSP, NULL);
                        return true;
                    }
                }
            }
            PowerDefaultOnSP.s = IPS_ALERT;
            return true;
        }
        
        // Focuser Mode
        if(!strcmp(name, FocuserModeSP.name))
        {
            snprintf(cmd, ASTROLINK4_LEN, "%s", "u");
            if(sendCommand(cmd, res))
            {
                std::string concatSettings = "U";
                std::vector<std::string> result = split(res, ":");
                if(result.size() >= 5)
                {
                    if(states[0] == ISS_ON) result[7] = "1";
                    if(states[1] == ISS_ON) result[7] = "2";
                    if(states[2] == ISS_ON) result[7] = "3";
                    for (const auto &piece : result) concatSettings += ":" + piece;
                    snprintf(cmd, ASTROLINK4_LEN, "%s", concatSettings.c_str());
                    LOG_ERROR(concatSettings.c_str());
                    if(sendCommand(cmd, res))
                    {
                        FocuserModeSP.s = IPS_BUSY;
                        IUUpdateSwitch(&FocuserModeSP, states, names, n);
                        IDSetSwitch(&FocuserModeSP, NULL);
                        return true;
                    }
                }
            }
            FocuserModeSP.s = IPS_ALERT;
            return true;
        }

        if (strstr(name, "FOCUS"))
            return FI::processSwitch(dev, name, states, names, n);

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
            PowerLabelsTP.s = IPS_OK;
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
    FI::saveConfigItems(fp);
    
    IUSaveConfigText(fp, &PowerLabelsTP);
    return true;
}

//////////////////////////////////////////////////////////////////////
/// PWM outputs
//////////////////////////////////////////////////////////////////////
bool IndiAstrolink4::setAutoPWM()
{
    char cmd[ASTROLINK4_LEN] = {0}, res[ASTROLINK4_LEN] = {0};
    bool allOk = true;

    uint8_t valA = (AutoPWMS[0].s == ISS_ON) ? 255 : static_cast<uint8_t>(PWMN[0].value);
    uint8_t valB = (AutoPWMS[1].s == ISS_ON) ? 255 : static_cast<uint8_t>(PWMN[1].value);

    snprintf(cmd, ASTROLINK4_LEN, "B:0:%d", valA);
    allOk = allOk && sendCommand(cmd, res);
    snprintf(cmd, ASTROLINK4_LEN, "B:1:%d", valB);
    allOk = allOk && sendCommand(cmd, res);

    return allOk;
}

//////////////////////////////////////////////////////////////////////
/// Focuser interface
//////////////////////////////////////////////////////////////////////
IPState IndiAstrolink4::MoveAbsFocuser(uint32_t targetTicks)
{
    char cmd[ASTROLINK4_LEN] = {0}, res[ASTROLINK4_LEN] = {0};
    snprintf(cmd, ASTROLINK4_LEN, "R:0:%u", targetTicks);
    lastMoveDirection = (FocusAbsPosN[0].value > targetTicks)? FOCUS_INWARD : FOCUS_OUTWARD;
    return (sendCommand(cmd, res)) ? IPS_BUSY : IPS_ALERT;
}

IPState IndiAstrolink4::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    return MoveAbsFocuser(dir == FOCUS_INWARD ? FocusAbsPosN[0].value - ticks : FocusAbsPosN[0].value + ticks);
}

bool IndiAstrolink4::AbortFocuser()
{
    char res[ASTROLINK4_LEN] = {0};
    return (sendCommand("H", res));
}

bool IndiAstrolink4::ReverseFocuser(bool enabled)
{
    char cmd[ASTROLINK4_LEN] = {0}, res[ASTROLINK4_LEN] = {0};
    snprintf(cmd, ASTROLINK4_LEN, "%s", "u");
    if(sendCommand(cmd, res))
    {
        std::string concatSettings = "U";
        std::vector<std::string> result = split(res, ":");
        if(result.size() >= 6)
        {
            result[6] = enabled ? "1" : "0";
            for (const auto &piece : result) concatSettings += ":" + piece;
            snprintf(cmd, ASTROLINK4_LEN, "%s", concatSettings.c_str());
            LOG_ERROR(concatSettings.c_str());
            if(sendCommand(cmd, res)) return true;
        }
    }
    return false;
}

bool IndiAstrolink4::SyncFocuser(uint32_t ticks)
{
    char cmd[ASTROLINK4_LEN] = {0}, res[ASTROLINK4_LEN] = {0};
    snprintf(cmd, ASTROLINK4_LEN, "P:%u", ticks);
    return sendCommand(cmd, res);
}

bool IndiAstrolink4::SetFocuserBacklash(int32_t steps)
{
    backlashSteps = steps;
    return true;
}

bool IndiAstrolink4::SetFocuserBacklashEnabled(bool enabled)
{
    backlashEnabled = enabled;
}

//////////////////////////////////////////////////////////////////////
/// Serial commands
//////////////////////////////////////////////////////////////////////
bool IndiAstrolink4::sendCommand(const char * cmd, char * res)
{
    int nbytes_read = 0, nbytes_written = 0, tty_rc = 0;
    char command[ASTROLINK4_LEN];

    if(isSimulation())
    {
        if(strcmp(cmd, "#") == 0) sprintf(res, "%s\n", "#:AstroLink4mini");
        if(strcmp(cmd, "q") == 0) sprintf(res, "%s\n", "q:1234:0:1.47:1:2.12:45.1:-12.81:0:0:45:0:0:0:1:12.1:5.0:1.12:13.41:0:34:0:0");
        if(strcmp(cmd, "p") == 0) sprintf(res, "%s\n", "p:1234");
        if(strcmp(cmd, "i") == 0) sprintf(res, "%s\n", "i:0");
        if(strncmp(cmd, "R", 1) == 0) sprintf(res, "%s\n", "R:");
        if(strncmp(cmd, "C", 1) == 0) sprintf(res, "%s\n", "C:");
        if(strncmp(cmd, "B", 1) == 0) sprintf(res, "%s\n", "B:");
        if(strncmp(cmd, "H", 1) == 0) sprintf(res, "%s\n", "H:");
        if(strncmp(cmd, "P", 1) == 0) sprintf(res, "%s\n", "P:");
        if(strncmp(cmd, "u", 1) == 0) sprintf(res, "%s\n", "u:25000:220:0:100:440:0:2:1:257:0:0:0:0:0:1:0:0");
        if(strncmp(cmd, "U", 1) == 0) sprintf(res, "%s\n", "U:");
        if(strncmp(cmd, "S", 1) == 0) sprintf(res, "%s\n", "S:");
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
/// Sensors
//////////////////////////////////////////////////////////////////////
bool IndiAstrolink4::sensorRead()
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
        
        PowerDataN[2].value = std::stod(result[3]);
        if(result.size() > 4)
        {
            float focuserPosition = std::stod(result[1]);
            float stepsToGo = std::stod(result[2]);
            if((FocusAbsPosN[0].value != focuserPosition) && (stepsToGo > 0))
            {
                FocusAbsPosN[0].value = focuserPosition;
                if(stepsToGo > 0)
                {
                    FocusAbsPosNP.s = FocusRelPosNP.s = IPS_BUSY;
                }
                else
                {
                    FocusAbsPosNP.s = FocusRelPosNP.s = IPS_OK;
                }
                IDSetNumber(&FocusAbsPosNP, nullptr);
                IDSetNumber(&FocusRelPosNP, nullptr);
            }
            
            if(std::stod(result[4]) > 0)
            {
                setParameterValue("WEATHER_TEMPERATURE", std::stod(result[5]));
                setParameterValue("WEATHER_HUMIDITY", std::stod(result[6]));
                setParameterValue("WEATHER_DEWPOINT", std::stod(result[7]));
                ParametersNP.s = IPS_OK;
                IDSetNumber(&ParametersNP, NULL);
            }
            else
            {
                ParametersNP.s = IPS_IDLE;
            }
                
            if(std::stod(result[8]) > 0)
            {
                Sensor2N.value = std::stod(result[9]);
                Sensor2NP.s = IPS_OK;
                IDSetNumber(&Sensor2NP, NULL);
            }
            else
            {
                Sensor2NP.s = IPS_IDLE;
            }
                
            float pwmA = std::stod(result[10]);
            float pwmB = std::stod(result[11]);
            if(PWMN[0].value != pwmA || PWMN[1].value != pwmB)
            {
                PWMN[0].value = pwmA;
                PWMN[1].value = pwmB;
                PWMNP.s=IPS_OK;
                IDSetNumber(&PWMNP, NULL);
            }
            
            bool dcMotorMoving = (std::stod(result[19]) > 0);
            if(dcMotorMoving)
            {
                DCFocStartSP.s = IPS_BUSY;
                IDSetSwitch(&DCFocStartSP, NULL);
            }
            else if (DCFocStartSP.s == IPS_BUSY)
            {
                DCFocStartSP.s = IPS_OK;
                DCFocAbortSP.s = IPS_IDLE;
                IDSetSwitch(&DCFocStartSP, NULL);
                IDSetSwitch(&DCFocAbortSP, NULL);
            }
            
            ISState power1 = (std::stod(result[12]) > 0) ? ISS_ON : ISS_OFF;
            ISState power2 = (std::stod(result[13]) > 0) ? ISS_ON : ISS_OFF;
            ISState power3 = (std::stod(result[14]) > 0) ? ISS_ON : ISS_OFF;
            if(Power1S[0].s != power1 || Power2S[0].s != power2 || Power3S[0].s != power3)
            {
                Power1S[0].s = (power1 == ISS_ON) ? ISS_ON : ISS_OFF;
                Power1S[1].s = (power1 == ISS_OFF) > 0) ? ISS_ON : ISS_OFF;
                Power1SP.s = IPS_OK;
                IDSetSwitch(&Power1SP, NULL);
                Power2S[0].s = (power2 == ISS_ON) ? ISS_ON : ISS_OFF;
                Power2S[1].s = (power2 == ISS_OFF) > 0) ? ISS_ON : ISS_OFF;
                Power2SP.s = IPS_OK;
                IDSetSwitch(&Power2SP, NULL);
                Power3S[0].s = (power3 == ISS_ON) ? ISS_ON : ISS_OFF;
                Power3S[1].s = (power3 == ISS_OFF) ? ISS_ON : ISS_OFF;
                Power3SP.s = IPS_OK;
                IDSetSwitch(&Power3SP, NULL);
            }
            
            float compensationVal = std::stod(result[20]);
            if(CompensationValueN[0].value != compensationVal)
            {
                CompensationValueN[0].value = compensationVal;
                IPState newState = (CompensationValueN[0].value > 0) ? IPS_OK : IPS_IDLE;
                CompensationValueNP.s = newState;
                CompensateNowSP.s = newState;
                CompensateNowS[0].s = ISS_OFF;
                IDSetNumber(&CompensationValueNP, NULL);
                IDSetSwitch(&CompensateNowSP, NULL);
            }
            
            PowerDataN[0].value = std::stod(result[15]);
            PowerDataN[1].value = std::stod(result[16]);
            PowerDataN[3].value = std::stod(result[17]);
            PowerDataN[4].value = std::stod(result[18]);
        }
        PowerDataNP.s=IPS_OK;
        IDSetNumber(&PowerDataNP, NULL);
    }
    
    // update settings data if was changed
    if(FocuserSettingsNP.s != IPS_OK || FocuserModeSP != IPS_OK || PowerDefaultOnSP.s != IPS_OK || OtherSettingsNP.s != IPS_OK)
    {
        if (sendCommand("u", res))
        {
            std::vector<std::string> result = split(res, ":");
            
            if(!strcmp("1", result[7].c_str())) FocuserModeS[FS_MODE_UNI].s = ISS_ON;
            if(!strcmp("2", result[7].c_str())) FocuserModeS[FS_MODE_BI].s = ISS_ON;
            if(!strcmp("3", result[7].c_str())) FocuserModeS[FS_MODE_MICRO].s = ISS_ON;
            FocuserModeSP.s = IPS_OK;
            IDSetSwitch(&FocuserModeSP, NULL);
            
            PowerDefaultOnS[0].s = (std::stod(result[13]) > 0) ? ISS_ON : ISS_OFF;
            PowerDefaultOnS[1].s = (std::stod(result[14]) > 0) ? ISS_ON : ISS_OFF;
            PowerDefaultOnS[2].s = (std::stod(result[15]) > 0) ? ISS_ON : ISS_OFF;
            PowerDefaultOnSP.s = IPS_OK;
            IDSetSwitch(&PowerDefaultOnSP, NULL);
            
            FocuserSettingsN[FS_MAX_POS].value = std::stod(result[1]);
            FocuserSettingsN[FS_SPEED].value = std::stod(result[2]);
            FocuserSettingsN[FS_STEP_SIZE].value = std::stod(result[9]) / 100.0;
            FocuserSettingsNP.s = IPS_OK;
            IDSetNumber(&FocuserSettingsNP, NULL);
            
            OtherSettingsN[SET_AREF_COEFF].value = std:stod(result[10]);
            OtherSettingsN[SET_OVER_TIME].value = std:stod(result[11]);
            OtherSettingsN[SET_OVER_VOLT].value = std:stod(result[12]);
            OtherSettingsN[SET_OVER_AMP].value = std:stod(result[13]);
            OtherSettingsNP.s = IPS_OK;
            IDSetNumber(&OtherSettingsNP, NULL);
            
        }
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
}

//////////////////////////////////////////////////////////////////////
/// Helper functions
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

std::string IndiAstrolink4::doubleToStr(double val)
{
    char * buf[10];
    sprintf(buf, "%.f0", val);
    std:string ret(1, buf);
    return ret;
}
