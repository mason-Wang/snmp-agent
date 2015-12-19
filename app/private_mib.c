/*  private_mib.h
 *  Copyright (c) 2012 Joe Rawson, MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
 * and associated documentation files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, 
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or 
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * required by mib2.c for using private MIBs.
 *
 * I believe the private_mib mechanism in lwip is fundamentally broken
 * for enterprise mibs. To preserve clean code and to modify lwip as little
 * as possible, comment out #includes for private_mib.h in snmp_structs.h
 * and snmp_msg.h and #include private_mib.h only in mib2.c, set LWIP_SNMP
 * and SNMP_PRIVATE_MIB in opt.h, and put your entire implementation in
 * private_mib.h. The reason for this is that it will be included in mib2.c
 * and trying to have a separate private_mib.c doesn't seem
 * to work for linking purposes.
 *
 * Author Joe Rawson
 *
 * The following implementation is loosely based on:
 * http://mbed.org/users/lorcansmith/code/Enet_SPI/ by Lorcan Smith
 * With many modifications for our specific implementation.
 */
 
#ifndef        __PRIVATE_MIB_H__
#define        __PRIVATE_MIB_H__
 
#include "../lwip-1.3.0/src/include/lwip/opt.h"
#include "../lwip-1.3.0/src/include/lwip/snmp_asn1.h"
#include "../lwip-1.3.0/src/include/lwip/snmp_structs.h"
#include "hw_memmap.h"
#include "hw_types.h"
#include "gpio.h"

 
#if SNMP_PRIVATE_MIB
 
#ifdef __cplusplus
extern "C" {
#endif
 
#define        PSU_ENTERPRISE_ID   34509 // PSU, registered by
                                         // Jim Stapleton
#define        TheCAT_ORG_ID    200     // Assigned to theCAT by
                                        // Jim Stapleton
#define        SNMP_ID          161     // Assigned to SNMP agents by Dave Burns for theCAT.
#define        BACON_ID         1       // Assigned to BACON by Dave Burns.
#define        NUM_OF_SENSORS   32       // the number of sensors BACON has.
 
// global variables we are returning to the NMS
u32_t led1 = 0, led2 = 0, beep = 0;
 
/********************************************************************
 * Function declarations
 *******************************************************************/
 
// returns the definition of the object
void BACON_get_obj_def(u8_t id_len, s32_t *id, struct obj_def *rv);
 
// returns the value of the object
void BACON_get_obj_val(struct obj_def *oid, u16_t length, void *value);
 
 
/******************************************************************************
 * BACON_get_obj_def
 * Description: Sets the object definition for the sensors
 * Parameters: u8_t id_len - length of branch id being given to us
 *             s32_t *ident - pointer to array holding the id
               struct obj_def *rv - struct we are returning our answer to
 * Returns: through *rv, the definition of the object scalar being queried
 ******************************************************************************/
void BACON_get_obj_def(u8_t id_len, s32_t *id, struct obj_def *rv) {
 
    u8_t oid = 0;
 
    id_len += 1;
    id -= 1;
    LWIP_ASSERT("invalid id", (id[0] >= 0) && (id[0] <= 0xff));
    if (id_len == 2) {
        rv->id_inst_len = id_len;
        rv->id_inst_ptr = id;
        oid = (u8_t)id[0];
        switch(oid) {
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18: 
            rv->instance    = MIB_OBJECT_SCALAR;
            rv->access    = MIB_OBJECT_READ_WRITE;
            rv->asn_type    = (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG);
            rv->v_len    = sizeof(u32_t);
            break;
        default: 
            rv->instance    = MIB_OBJECT_SCALAR;
            rv->access    = MIB_OBJECT_READ_ONLY;
            rv->asn_type    = (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG);
            rv->v_len    = sizeof(u32_t);
            break;
        }
    } else {
        LWIP_DEBUGF(SNMP_MIB_DEBUG,("\r\nBACON_get_obj_def: no scalar\r\n"));
        rv->instance = MIB_OBJECT_NONE;
    }
}
 
 
/******************************************************************************
 * BACON_get_obj_val
 * Description: Sets the returned value for the sensors
 * Parameters: struct obj_def *od - object found from obj_def
 *             u16_t length - the length of what we are being asked (in bytes)
 *             void *value - points to (varbind) space to copy value into
 * Returns: in *value
 ******************************************************************************/
void BACON_get_obj_val(struct obj_def *od, u16_t length, void *value) {
    
    u8_t oid = 0;
 
    u32_t *int_ptr = (u32_t*)value;
 
    //LWIP_UNUSED_ARG(length);
    LWIP_ASSERT("invalid id", (od->id_inst_ptr[0] >=0) && (od->id_inst_ptr[0] <= NUM_OF_SENSORS));
    
    oid = (u8_t)od->id_inst_ptr[0];
    switch(oid) {
    case 1: //FIBER PE3
        *int_ptr = (GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_3) & 0xff) >> 3;
        break;
    case 2: // RX_LOS PB2
        *int_ptr = (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2) & 0xff) >> 2;
        break;
    case 3: // BAUD1_1 PB0
        *int_ptr = (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_0) & 0xff);
        break;
    case 4: // BAUD1_2 PF1
        *int_ptr = (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1) & 0xff) >> 1;
		break;
	case 5: // BAUD1_3 PF2
		*int_ptr = (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_2) & 0xff) >> 2;
        break;
    case 6: // BAUD1_4 PF3
        *int_ptr = (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_3) & 0xff) >> 3;
        break;
    case 7: // BAUD1_1_R PD3
        *int_ptr = (GPIOPinRead(GPIO_PORTD_BASE, GPIO_PIN_3) & 0xff) >> 3;
        break;
    case 8: // BAUD1_2_R PD2
        *int_ptr = (GPIOPinRead(GPIO_PORTD_BASE, GPIO_PIN_2) & 0xff) >> 2;
        break;
    case 9: // BAUD1_3_R PD1
        *int_ptr = (GPIOPinRead(GPIO_PORTD_BASE, GPIO_PIN_1) & 0xff) >> 1;
        break;
    case 10: // BAUD1_4_R PD0
        *int_ptr = GPIOPinRead(GPIO_PORTD_BASE, GPIO_PIN_0) & 0xff;
        break;
    case 11: // BAUD2_1 PA7
        *int_ptr = (GPIOPinRead(GPIO_PORTA_BASE, GPIO_PIN_7) & 0xff) >> 7;
        break;
    case 12: // BAUD2_2 PA6
        *int_ptr = (GPIOPinRead(GPIO_PORTA_BASE, GPIO_PIN_6) & 0xff) >> 6;
        break;
    case 13: // BAUD2_3 PA5
        *int_ptr = (GPIOPinRead(GPIO_PORTA_BASE, GPIO_PIN_5) & 0xff) >> 5;
        break;
    case 14: // BAUD2_4 PA4
        *int_ptr = (GPIOPinRead(GPIO_PORTA_BASE, GPIO_PIN_4) & 0xff) >> 4;
        break;
    case 15: // BAUD2_1_R PE4
        *int_ptr = (GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_4) & 0xff) >> 4;
        break;
    case 16: // BAUD2_2_R PE5
        *int_ptr = (GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_5) & 0xff) >> 5;
        break;
    case 17: // BAUD2_3_R PE6
        *int_ptr = (GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_6) & 0xff) >> 6;
        break;
    case 18: // BAUD2_4_R PE7
        *int_ptr = (GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_7) & 0xff) >> 7;
        break;
    case 19: // TP_Link1 PC7
        *int_ptr = (GPIOPinRead(GPIO_PORTC_BASE, GPIO_PIN_7) & 0xff) >> 7;
        break;
    case 20: // TP_Link2 PC6
        *int_ptr = (GPIOPinRead(GPIO_PORTC_BASE, GPIO_PIN_6) & 0xff) >> 6;
        break;
    case 21: // TP_Link3 PC5
        *int_ptr = (GPIOPinRead(GPIO_PORTC_BASE, GPIO_PIN_5) & 0xff) >> 5;
        break;
    case 22: // TP_Link4 PC4
        *int_ptr = (GPIOPinRead(GPIO_PORTC_BASE, GPIO_PIN_4) & 0xff) >> 4;
        break;
    case 23: // Far_TP_Link1 PB1
        *int_ptr = (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_1) & 0xff) >> 1;
        break;
    case 24: // Far_TP_Link2 PB3
        *int_ptr = (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_3) & 0xff) >> 3;
        break;
    case 25: // Far_TP_Link3 PE0
        *int_ptr = GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_0) & 0xff;
        break;
    case 26: // Far_TP_Link4 PE1
        *int_ptr = (GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_1) & 0xff) >> 1;
        break;
    case 27: // STATUS1 PA2
        *int_ptr = (GPIOPinRead(GPIO_PORTA_BASE, GPIO_PIN_2) & 0xff) >> 2;
        break;
    case 28: // STATUS2 PA3
        *int_ptr = (GPIOPinRead(GPIO_PORTA_BASE, GPIO_PIN_3) & 0xff) >> 3;
        break;
    case 29: // RXD1_MON PD6
        *int_ptr = (GPIOPinRead(GPIO_PORTD_BASE, GPIO_PIN_6) & 0xff) >> 6;
        break;
    case 30: // TXD1_MON PD7
        *int_ptr = (GPIOPinRead(GPIO_PORTD_BASE, GPIO_PIN_7) & 0xff) >> 7;
        break;
    case 31: // RXD2_MON PD4
        *int_ptr = (GPIOPinRead(GPIO_PORTD_BASE, GPIO_PIN_4) & 0xff) >> 4;
        break;
    case 32: // TXD2_MON PD5
        *int_ptr = (GPIOPinRead(GPIO_PORTD_BASE, GPIO_PIN_5) & 0xff) >> 5;
        break;
    default:
        break;
    }
}

static u8_t BACON_set_test(struct obj_def *od, u16_t len, void *value)
{
	u8_t id, set_ok;

	set_ok = 0;
	id = od->id_inst_ptr[0];
	switch(id) {
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
        set_ok = 1;
        break;
    default:
        break;
	}
	
	return set_ok;
}
 
static void BACON_set_value(struct obj_def *od, u16_t len, void *value)
{
	u8_t id;
	u32_t val = *((u32_t *)value);
	
	id = od->id_inst_ptr[0];
	switch (id) {    
    case 3: // BAUD1_1 PB0
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, (val & 0xff ? 1 : 0));
        break;
    case 4: // BAUD1_2 PF1
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, (val & 0xff ? 1 : 0) << 1);
        break;
    case 5: // BAUD1_3 PF2
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, (val & 0xff ? 1 : 0) << 2);
        break;
    case 6: // BAUD1_4 PF3
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, (val & 0xff ? 1 : 0) << 3);
        break;
    case 7: // BAUD1_1_R PD3
        GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_3, (val & 0xff ? 1 : 0) << 3);
        break;
    case 8: // BAUD1_2_R PD2
        GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, (val & 0xff ? 1 : 0) << 2);
        break;
    case 9: // BAUD1_3_R PD1
        GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_1, (val & 0xff ? 1 : 0) << 1);
        break;
    case 10: // BAUD1_4_R PD0
        GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0, (val & 0xff ? 1 : 0));
        break;
    case 11: // BAUD2_1 PA7
        GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_7, (val & 0xff ? 1 : 0) << 7);
        break;
    case 12: // BAUD2_2 PA6
        GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_6, (val & 0xff ? 1 : 0) << 6);
        break;
    case 13: // BAUD2_3 PA5
        GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_5, (val & 0xff ? 1 : 0) << 5);
        break;
    case 14: // BAUD2_4 PA4
        GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_4, (val & 0xff ? 1 : 0) << 4);
        break;
    case 15: // BAUD2_1_R PE4
        GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_4, (val & 0xff ? 1 : 0) << 4);
        break;
    case 16: // BAUD2_2_R PE5
        GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_5, (val & 0xff ? 1 : 0) << 5);
        break;
    case 17: // BAUD2_3_R PE6
        GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_6, (val & 0xff ? 1 : 0) << 6);
        break;
    case 18: // BAUD2_4_R PE7
        GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_7, (val & 0xff ? 1 : 0) << 7);
        break;
    default:
        break;
	}
}
 
/********************************************************************
 * MIB structures
 *******************************************************************/
 
// defines the scalars used to return sensor values.
const mib_scalar_node BACON_sensor = {
    &BACON_get_obj_def,
    &BACON_get_obj_val,
    &BACON_set_test,
    &BACON_set_value,
    MIB_NODE_SC,
    0
};
 
// The OIDs for the sensor scalars.
const s32_t BACON_sensor_oids[32] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
        13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
// The actual structure that holds the nodes.
struct mib_node* const BACON_sensor_nodes[32] = {
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor,
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor, 
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor,
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor,
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor,
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor, 
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor,
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor,
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor,
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor, 
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor,
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor,
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor,
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor, 
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor,
(struct mib_node*)&BACON_sensor, (struct mib_node*)&BACON_sensor,
};
 
// 1.3.6.1.4.1.34509.200.161.1.[12345]
const struct mib_array_node BACON_sensors = {
    &noleafs_get_object_def,
    &noleafs_get_value,
    &noleafs_set_test,
    &noleafs_set_value,
    MIB_NODE_AR,
    32,
    BACON_sensor_oids,
    BACON_sensor_nodes
};
 
// putting them together.
const s32_t BACON_oids[1] = { BACON_ID };
struct mib_node* const BACON_nodes[1] = {
    (struct mib_node*)&BACON_sensors
};
// 1.3.6.1.4.1.34509.200.161.1
const struct mib_array_node BACON_mib = {
    &noleafs_get_object_def,
    &noleafs_get_value,
    &noleafs_set_test,
    &noleafs_set_value,
    MIB_NODE_AR,
    1,
    BACON_oids,
    BACON_nodes
};
 
 
// 1.3.6.1.4.1.34509.200.161
// defines BACON
const s32_t BACON_ids[1] = { SNMP_ID };
struct mib_node* const BACON_sections[1] = {(struct mib_node*)&BACON_mib};
const struct mib_array_node mib_BACON = {
    &noleafs_get_object_def,
    &noleafs_get_value,
    &noleafs_set_test,
    &noleafs_set_value,
    MIB_NODE_AR,
    1,
    BACON_ids,
    BACON_sections
};
 
// 1.3.6.1.4.1.34509.200
// defines TheCAT
const s32_t CAT_oids[3] = { TheCAT_ORG_ID };
struct mib_node* const CAT_nodes[1] = {(struct mib_node*)&mib_BACON};
const struct mib_array_node CAT_mib = {
    &noleafs_get_object_def,
    &noleafs_get_value,
    &noleafs_set_test,
    &noleafs_set_value,
    MIB_NODE_AR,
    1,
    CAT_oids,
    CAT_nodes
};
 
// 1.3.6.1.4.1.34509
// defines PSU_ENTERPRISE_ID
const s32_t private_oids[1] = { PSU_ENTERPRISE_ID };
struct mib_node* const private_nodes[1] = {(struct mib_node*)&CAT_mib};
// performs the linkage to the enterprise branch.
const struct mib_array_node mib_enterprise = {
    &noleafs_get_object_def,
    &noleafs_get_value,
    &noleafs_set_test,
    &noleafs_set_value,
    MIB_NODE_AR,
    1,
    private_oids,
    private_nodes
};
 
// 1.3.6.1.4.1
// links our mibs to the private branch
const s32_t ent_oids[1] = { 1 };
struct mib_node* const ent_nodes[1] = { (struct mib_node*)&mib_enterprise };
 
 
const struct mib_array_node private = {
    &noleafs_get_object_def,
    &noleafs_get_value,
    &noleafs_set_test,
    &noleafs_set_value,
    MIB_NODE_AR,
    1,
    ent_oids,
    ent_nodes
};
 
#ifdef __cplusplus
}
#endif
 
#endif       // SNMP_PRIVATE_MIB
#endif       // __PRIVATE_MIB_H__
 