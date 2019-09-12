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
#include "astrolink4.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <memory>
#include <regex>
#include <termios.h>
#include <cstring>
#include <sys/ioctl.h>
#include <chrono>
#include <iomanip>

#define TIMERDELAY 500

std::unique_ptr<AstroLink4> astroLink4(new AstroLink4());

////////////////////////////////////////////////////////////////////////////
/// 
////////////////////////////////////////////////////////////////////////////
void ISGetProperties(const char *dev)
{
    astroLink4->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    astroLink4->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    astroLink4->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    astroLink4->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    astroLink4->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice(XMLEle *root)
{
    INDI_UNUSED(root);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
AstroLink4::AstroLink4()
{
    setVersion(0, 1);
}

const char *AstroLink4::getDefaultName()
{
    return "AstroLink 4";
}

bool AstroLink4::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE);
    
    addSimulationControl();
    
    IUFillNumber(&FocuserPosReadN[0], "FOCUS_ABSOLUTE_POSITION", "Steps", "%06d", 0, MAX_STEPS, 50, 0);
    IUFillNumberVector(&FocuserPosReadNP, FocuserPosReadN, 1, getDeviceName(), "FOCUS_POS", "Focuser Position", FOCUS_TAB, IP_RO, 0, IPS_OK);
    
    IUFillNumber(&FocuserMoveToN[0], "FOCUS_MOVEMENT", "Steps", "%06d", 0, MAX_STEPS, 50, 0);
    IUFillNumberVector(&FocuserMoveToNP, FocuserMoveToN, 1, getDeviceName(), "FOCUS_MOV", "Move To", FOCUS_TAB, IP_WO, 0, IPS_OK);
}

bool AstroLink4::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
    	defineNumber(&FocusAbsPosNP);
		defineNumber(&FocuserMoveToNP);
    }
    else
    {
    	deleteProperty(FocusAbsPosNP.name);
		deleteProperty(FocuserMoveToNP.name);
    }
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool AstroLink4::sendMoveTo(uint8_t newPos)
{
    char cmd[10] = {0}, res[10] = {0};
    snprintf(cmd, 10, "R:0:%d", newPos);
    if (sendCommand(cmd, res))
    {
        return true;
    }
    return false;
}


bool AstroLink4::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
		// handle focuser move to
		if (!strcmp(name, FocuserMoveToNP.name))
		{
		    if (sendMoveTo(static_cast<uint8_t>(values[0])))
            {
                IUUpdateNumber(&FocuserMoveToNP, values, names, n);
                FocuserMoveToNP.s = IPS_OK;
            }
            else
                FocuserMoveToNP.s = IPS_ALERT;

            return true;
		}        
    }        
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void AstroLink4::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(TIMERDELAY);
        return;
    }
	readDeviceData();
    SetTimer(TIMERDELAY);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool AstroLink4::readDeviceData()
{
    char res[ASTROLINK4_LEN] = {0};
    if (sendCommand("q", res))
    {
        std::vector<std::string> result = split(res, ":");

        FocuserPosReadN[0].value = std::stod(result[1]);
        IDSetNumber(&FocuserPosReadNP, NULL);

		return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool AstroLink4::Handshake()
{
    char response[ASTROLINK4_LEN] = {0};
    int nbytes_read = 0;
    PortFD = serialConnection->getPortFD();

    LOG_DEBUG("CMD <#>");

    if (isSimulation())
    {
        snprintf(response, ASTROLINK4_LEN, "#:AstroLink4mini"");
        nbytes_read = 8;
    }
    else
    {
        int tty_rc = 0, nbytes_written = 0;
        char command[ASTROLINK4_LEN] = {0};
        tcflush(PortFD, TCIOFLUSH);
        strncpy(command, "#\n", ASTROLINK4_LEN);
        if ( (tty_rc = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
        {
            char errorMessage[MAXRBUF];
            tty_error_msg(tty_rc, errorMessage, MAXRBUF);
            LOGF_ERROR("Serial write error: %s", errorMessage);
            return false;
        }

        if ( (tty_rc = tty_nread_section(PortFD, response, ASTROLINK4_LEN, stopChar, 1, &nbytes_read)) != TTY_OK)
        {
            // Try once more
            if (tty_rc == TTY_OVERFLOW || tty_rc == TTY_TIME_OUT)
            {
                tcflush(PortFD, TCIOFLUSH);
                tty_write_string(PortFD, command, &nbytes_written);
                tty_rc = tty_nread_section(PortFD, response, ASTROLINK4_LEN, stopChar, 1, &nbytes_read);
            }

            if (tty_rc != TTY_OK)
            {
                char errorMessage[MAXRBUF];
                tty_error_msg(tty_rc, errorMessage, MAXRBUF);
                LOGF_ERROR("Serial read error: %s", errorMessage);
                return false;
            }
        }

        tcflush(PortFD, TCIOFLUSH);
    }

    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);
    
	if(strcmp(response, "#:AstroLink4mini") != 0)
	{
		LOGF_ERROR("Device not recognized.");
		return false;
	}

    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool AstroLink4::sendCommand(const char * cmd, char * res)
{
    int nbytes_read = 0, nbytes_written = 0, tty_rc = 0;
    LOGF_DEBUG("CMD <%s>", cmd);

    if (isSimulation())
    {
        if (!strcmp(cmd, "p"))
        {
            strncpy(res, "p:1234", ASTROLINK4_LEN);
        }
        else if (res)
        {
            strncpy(res, cmd, ASTROLINK4_LEN);
        }
        return true;
    }

    for (int i = 0; i < 2; i++)
    {
        char command[ASTROLINK4_LEN] = {0};
        tcflush(PortFD, TCIOFLUSH);
        snprintf(command, ASTROLINK4_LEN, "%s\n", cmd);
        if ( (tty_rc = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
            continue;

        if (!res)
        {
            tcflush(PortFD, TCIOFLUSH);
            return true;
        }

        if ( (tty_rc = tty_nread_section(PortFD, res, ASTROLINK4_LEN, stopChar, ASTROLINK4_TIMEOUT, &nbytes_read)) != TTY_OK || nbytes_read == 1)
            continue;

        tcflush(PortFD, TCIOFLUSH);
        res[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES <%s>", res);
        return (cmd[0] == res[0]);
    }

    if (tty_rc != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(tty_rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial error: %s", errorMessage);
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool AstroLink4::saveConfigItems(FILE * fp)
{
	INDI::DefaultDevice::saveConfigItems(fp);
	
	IUSaveConfigNumber(fp, &FocuserMoveToN);
	return true;
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
