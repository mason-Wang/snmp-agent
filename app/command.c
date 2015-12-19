//*****************************************************************************
//
// command.c - parse the commands user input from uart0
//
//*****************************************************************************
#include "hw_types.h"
#include "uartstdio.h"
#include "sysctl.h"
#include "lwiplib.h"
#include <string.h>
#include <stdio.h>
#include "softeeprom_wrapper.h"
#include "storage_config.h"

#define MAXARGS	6
#define MAXARGLEN 31

extern void DisplayIPAddress(unsigned long ipaddr, char *type);

struct command
{
	char *keyword;
	int (*func)(int nargs, char **args);
	char *desc;
};

int help(int nargs, char **args);

int systemReset(int nargs, char **args)
{
	SysCtlReset();
	return 0;
}

int setIpAddr(int nargs, char **args)
{
	int ulIPAddress[4];
	int ulNetMask[4];
	int ulGateWay[4];
	int ret;
	unsigned long ulIp = 0;
	unsigned long ulNm = 0;
	unsigned long ulGw = 0;
	
	if (nargs <= 2 || nargs >= 5)
	{
		UARTprintf("Usage:setip ip netmask [gateway]\n");
	}
	else
	{
		ret = sscanf(args[1], "%d.%d.%d.%d", ulIPAddress, ulIPAddress + 1,
				ulIPAddress + 2, ulIPAddress + 3);
		if (ret == 4)
			ulIp = ulIPAddress[0] << 24 | ulIPAddress[1] << 16 | ulIPAddress[2] << 8 | ulIPAddress[3]; 
		
		ret = sscanf(args[2], "%d.%d.%d.%d", ulNetMask, ulNetMask + 1,
				ulNetMask + 2, ulNetMask + 3);
		if (ret == 4)
			ulNm = ulNetMask[0] << 24 | ulNetMask[1] << 16 | ulNetMask[2] << 8 | ulNetMask[3];
		
		if (nargs == 4)
		{
			ret = sscanf(args[3], "%d.%d.%d.%d", ulGateWay, ulGateWay + 1,
				ulGateWay + 2, ulGateWay + 3);
			if (ret == 4)
				ulGw = ulGateWay[0] << 24 | ulGateWay[1] << 16 | ulGateWay[2] << 8 | ulGateWay[3];
		}
		
		if (ulIp != 0 && ulNm != 0)
		{
			UARTprintf("setIpAddr:ip = 0x%08x, nm = 0x%08x, gw = 0x%08x\n", ulIp, ulNm, ulGw);
			// save the ip netmask and gateway to eeprom
			SoftEEPROM_WrapperWrite(EEPROM_IP_ADDR, sizeof(unsigned long), (unsigned char *)&ulIp);
			SoftEEPROM_WrapperWrite(EEPROM_NETMASK_ADDR, sizeof(unsigned long), (unsigned char *)&ulNm);
			SoftEEPROM_WrapperWrite(EEPROM_GATEWAY_ADDR, sizeof(unsigned long), (unsigned char *)&ulGw);
			
			// reset the system to enable the change
			SysCtlReset();
		}
		else 
		{
			UARTprintf("Usage:setip ip netmask [gateway]\n");
		}
	}

	return 0;
}

int setMacAddr(int nargs, char **args)
{
	int i, ret;
	unsigned int pucMACArray[6];
	unsigned char mac[6];
	
	if (nargs != 2)
	{
		UARTprintf("setmac macaddr(xx-xx-xx-xx-xx-xx)\n");
	}
	else
	{
		ret = sscanf(args[1], "%02x-%02x-%02x-%02x-%02x-%02x", pucMACArray, pucMACArray + 1, 
			pucMACArray + 2, pucMACArray + 3, pucMACArray + 4, pucMACArray + 5);
		if (ret == 6)
		{
			for (i = 0; i < 6; i++)
			{
				mac[i] = pucMACArray[i];
			}
			// save the mac address to eeprom
			SoftEEPROM_WrapperWrite(EEPROM_MAC_ADDR, 6, mac);
			
			// reset the system to enable the change
			SysCtlReset();
		}
		else 
		{
			UARTprintf("setmac macaddr(xx-xx-xx-xx-xx-xx)\n");
		}
	}
	
	return 0;
}

int getIpAddr(int nargs, char **args)
{
	unsigned long ulIPAddress;
	
	ulIPAddress = lwIPLocalIPAddrGet();
	DisplayIPAddress(ulIPAddress, "IP");
    ulIPAddress = lwIPLocalNetMaskGet();
    DisplayIPAddress(ulIPAddress, "MASK");
    ulIPAddress = lwIPLocalGWAddrGet();
    DisplayIPAddress(ulIPAddress, "GW");
	
	return 0;
}

int getMacAddr(int nargs, char **args)
{
	unsigned char mac[6];
	
	lwIPLocalMACGet(mac);
	UARTprintf("MAC: %02x-%02x-%02x-%02x-%02x-%02x\n", mac[0], mac[1],
			mac[2], mac[3], mac[4], mac[5]);
	
	return 0;
}

static const struct command cmd_tbl[] = 
{
	{"reset", 		systemReset, "Reset the system"},
	{"help", 		help,		"Check which commands support"},
	{"getip",	getIpAddr, 	"Get the ip address,netmask and gateway"},
	{"setip",	setIpAddr, 	"Set the ip address, netmask and gateway"},
	{"getmac",	getMacAddr, "Get the MAC address"},
	{"setmac",  setMacAddr, "Set the MAC address"},
};

int help(int nargs, char **args)
{
	int i;
	
	for (i = 0; i < sizeof(cmd_tbl)/sizeof(cmd_tbl[0]); i++)
	{
		UARTprintf("%s: %s\n", cmd_tbl[i].keyword, cmd_tbl[i].desc);
	}
	
	return 0;
}

void parseCmd(char *cmd, unsigned long len)
{
	int i;
	int nargs = 0;
	char args[MAXARGS][MAXARGLEN];
	char *pargs[MAXARGS];
	char *s, *c = cmd;
	
	// first remove the space before the arg
textresume:
	for (;;)
	{
		switch(*cmd)
		{
			case 0:
				goto textdone;
			case ' ':
				cmd++;
				continue;
			default:
				goto text;
		}
	}
	
text:
	s = args[nargs];
	for(;;)
	{
		switch(*cmd)
		{
			case 0:
				*s = 0;
				nargs++;
				goto textdone;
			case ' ':
				cmd++;
				nargs++;
				*s = 0;
				goto textresume;
			case '"':
				cmd++;
				break;
			default:
				*s++ = *cmd++;
				break;
		}
	}
	
textdone:
#if 0
	UARTprintf("parseCmd: %s, nargs = %d\n", c, nargs);
	for (i = 0; i < nargs; i++)
		UARTprintf("args[%d]:%s\n", i, args[i]);
#endif	
	
	for (i = 0; i < nargs; i++)
		pargs[i] = args[i];
	
	for (i = 0; i < sizeof(cmd_tbl)/sizeof(cmd_tbl[0]); i++)
	{
		if (!strcmp(cmd_tbl[i].keyword, args[0]))
		{
			cmd_tbl[i].func(nargs, (char **)pargs);
			break;
		}
	}
	
	if (i == sizeof(cmd_tbl)/sizeof(cmd_tbl[0]) && *c != 0)
		UARTprintf("Unknown command!Use help to check which commands support.\n");

	UARTprintf("lwip:");
}
