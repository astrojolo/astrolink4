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

#ifndef ASTROLINK4_H
#define ASTROLINK4_H

#include <string>
#include <iostream>
#include <stdio.h>

#include <defaultdevice.h>
#include <indifocuserinterface.h>
#include <indiweatherinterface.h>
#include <connectionplugins/connectionserial.h>

class IndiAstrolink4 : public INDI::DefaultDevice, public INDI::FocuserInterface, public INDI::WeatherInterface
{

public:
    IndiAstrolink4();
	virtual bool initProperties();
	virtual bool updateProperties();
	
	virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
	virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n);
	
protected:
	virtual const char *getDefaultName();
	virtual void TimerHit();
	virtual bool saveConfigItems(FILE *fp);
	Connection::Serial *serialConnection = NULL;
    virtual bool sendCommand(const char * cmd, char * res);

    // Focuser Overrides
    virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
    virtual bool AbortFocuser() override;
    virtual bool ReverseFocuser(bool enabled) override;
    virtual bool SyncFocuser(uint32_t ticks) override;

    virtual bool SetFocuserBacklash(int32_t steps) override;
    virtual bool SetFocuserBacklashEnabled(bool enabled) override;

    // Weather Overrides
    virtual IPState updateWeather() override
    {
        return IPS_OK;
    }

	
private:
	virtual bool Handshake();
	int PortFD = -1;
    std::vector<std::string> split(const std::string &input, const std::string &regex);
    bool sensorRead();
    bool setAutoPWM();
    char stopChar { 0xA };	// new line
    bool backlashEnabled = false;
    int32_t backlashSteps = 0;
    FocusDirection lastMoveDirection = FOCUS_INWARD;
    
	ISwitch Power1S[2];
	ISwitchVectorProperty Power1SP;
	ISwitch Power2S[2];
	ISwitchVectorProperty Power2SP;
	ISwitch Power3S[2];
	ISwitchVectorProperty Power3SP;
    
	INumber Sensor2N[1];
	INumberVectorProperty Sensor2NP;
    
	INumber PWMN[2];
	INumberVectorProperty PWMNP;

    ISwitch AutoPWMS[2];
    ISwitchVectorProperty AutoPWMSP;
    
    INumber PowerDataN[5];
    INumberVectorProperty PowerDataNP;

    INumber CompensationValueN[1];
    INumberVectorProperty CompensationValueNP;
    ISwitch CompensateNowS[1];
    ISwitchVectorProperty CompensateNowSP;
    
    IText PowerLabelsT[3] = {};
    ITextVectorProperty PowerLabelsTP;

    INumber FocuserSettingsN[4];
    INumberVectorProperty FocuserSettingsNP;
    enum
    {
        FS_MAX_POS, FS_SPEED, FS_STEP_SIZE, FS_COMPENSATION
    };
    ISwitch FocuserModeS[3];
    ISwitchVectorProperty FocuserModeSP;
    enum
    {
      FS_MODE_UNI, FS_MODE_BI, FS_MODE_MICRO
    };
    
	static constexpr const char *POWER_TAB {"Power"};
	static constexpr const char *ENVIRONMENT_TAB {"Environment"};
    static constexpr const char *SETTINGS_TAB {"Settings"};

};

#endif
