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
#pragma once

#include "defaultdevice.h"
#include <stdint.h>

#include <connectionplugins/connectionserial.h>

class AstroLink4 : public INDI::DefaultDevice
{
  public:
    AstroLink4();
    
    virtual bool initProperties() override;
    virtual bool updateProperties() override;    
    
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

  protected:
    const char *getDefaultName();
    virtual void TimerHit() override;
    
  private:
	bool Handshake();
	bool sendCommand(const char * cmd, char * res);
	bool readDeviceData();
    int PortFD { -1 };
    bool setupComplete { false };
    Connection::Serial *serialConnection { nullptr };
    char stopChar { 0xA };	// new line
    
    INumber FocuserPosReadN[1];
	INumberVectorProperty FocuserPosReadNP;
	
	INumber FocuserMoveToN[1];
	INumberVectorProperty FocuserMoveToNP;
	
	bool sendMoveTo(uint8_t newPos);
    
    static constexpr const uint8_t ASTROLINK4_TIMEOUT {3};
    static constexpr const uint8_t ASTROLINK4_LEN {128};    
    static constexpr const char *POWER_TAB {"Power"};
   	static constexpr const char *FOCUS_TAB {"Focuser"};
	static constexpr const char *ENVIRONMENT_TAB {"Environment"};
};