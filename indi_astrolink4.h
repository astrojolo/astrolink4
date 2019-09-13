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
#include <connectionplugins/connectionserial.h>

class IndiAstrolink4 : public INDI::DefaultDevice, public INDI::FocuserInterface
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

	
private:
	virtual bool Handshake();
	int PortFD = -1;
	int counter;
    std::vector<std::string> split(const std::string &input, const std::string &regex);
    char stopChar { 0xA };	// new line
    
	ISwitch Power1S[2];
	ISwitchVectorProperty Power1SP;
	ISwitch Power2S[2];
	ISwitchVectorProperty Power2SP;
	ISwitch Power3S[2];
	ISwitchVectorProperty Power3SP;
    
	INumber Sensor1N[3];
	INumberVectorProperty Sensor1NP;
    
	INumber PWMN[2];
	INumberVectorProperty PWMNP;
    
    INumber PowerDataN[5];
    INumberVectorProperty PowerDataNP;
    
    IText PowerLabelsT[3] = {};
    ITextVectorProperty PowerLabelsTP;
    
	static constexpr const char *POWER_TAB {"Power"};
	static constexpr const char *ENVIRONMENT_TAB {"Environment"};
    static constexpr const char *SETTINGS_TAB {"Settings"};
};

#endif
