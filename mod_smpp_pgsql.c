/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
*
* Version: MPL 1.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
*
* The Initial Developer of the Original Code is
* Anthony Minessale II <anthm@freeswitch.org>
* Portions created by the Initial Developer are Copyright (C)
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
* William King <william.king@quentustech.com>
* Dimitar Kokov <dkokov75@gmail.com>
*
*
*/

#include <mod_smpp.h>

switch_status_t mod_smpp_pgsql_connect(char *dsn)
{
    mod_smpp_globals.pgsql = switch_pgsql_handle_new(dsn);

    if(mod_smpp_globals.pgsql != NULL) {
	if(switch_pgsql_handle_connect(mod_smpp_globals.pgsql) == SWITCH_PGSQL_SUCCESS) {
	    return SWITCH_STATUS_SUCCESS;
	}
    }

    return SWITCH_STATUS_GENERR;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
