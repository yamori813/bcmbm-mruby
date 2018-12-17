/*
 * RoboSwitch setup functions
 *
 * Copyright 2005, Broadcom Corporation
 * All Rights Reserved.                
 *                                     
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;   
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior      
 * written permission of Broadcom Corporation.                            
 *
 * $Id: etc_robo.h,v 1.1.1.1 2006/09/11 12:40:26 anderson Exp $
 */

#ifndef _robo_h_
#define _robo_h_

/* forward declarations */
typedef struct robo_info_s robo_info_t;
typedef struct bcm4401_softc bcm4401_softc_t;
struct chops;

/* interface prototypes */
//extern robo_info_t *robo_attach(sb_t *sbh, struct chops *chop, void *ch, void *vars);
extern void robo_detach(robo_info_t *robo);
extern int robo_enable_device(robo_info_t *robo);
extern int robo_config_vlan(robo_info_t *robo, uint8_t *mac_addr);
extern int robo_enable_switch(robo_info_t *robo);

#endif /* _robo_h_ */
