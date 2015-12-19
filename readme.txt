SNMP agent use the Lwip TCP/IP stack

-- softeeprom.c softeeprom_wrapper.c  
	These two files is used to store data in the internal flash as a eeprom
-- command.c 
	This file include the commands that used to setup the board and get the board information like ip address,mac...
-- enet_lwip.c lwiplib.c
	The Lwip porting files
-- private_mib.c
	This file implement the snmp mib nodes

