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
*
* William King <william.king@quentustech.com>
* Dimitar Kokov <dkokov75@gmail.com>
*
* mod_smpp.c -- smpp client and server implementation using libsmpp
*
* using libsmpp from: http://cgit.osmocom.org/libsmpp/
*
*/

#ifndef MOD_SMPP_H
#define MOD_SMPP_H

#define BUFSZ 2048

#include <switch.h>
#include <smpp34.h>
#include <smpp34_structs.h>
#include <smpp34_params.h>

typedef struct mod_smpp_globals_s {
	switch_memory_pool_t *pool;
	switch_hash_t *gateways;
	uint8_t debug;
	
	switch_pgsql_handle_t *pgsql;
	switch_mutex_t *db_mutex;
} mod_smpp_globals_t;

extern mod_smpp_globals_t mod_smpp_globals;

typedef struct mod_smpp_gateway_s {
	char *name;
	char *host;
	char *system_id;
	char *password;
	char *profile;
	char *system_type;
	char address_range[64];
	uint32_t port;
	uint32_t sequence;
	uint32_t running;
	uint32_t debug;
	uint32_t data_coding_recognize;
	switch_socket_t *socket;
	switch_sockaddr_t *socketaddr;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr;
	switch_mutex_t *conn_mutex;
	switch_memory_pool_t *pool;
	
	int reconn_retry_sec;
	
	int mon_ts;
	int mon_thread_timer;
	time_t last_recv;
	time_t last_send;
	switch_thread_t *mon_thread;
	switch_threadattr_t *mon_thd_attr;
	switch_mutex_t *mon_mutex;
	char sip_smpp_gw_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1];
} mod_smpp_gateway_t;

typedef struct mod_smpp_message_s {
  submit_sm_t      req;
  submit_sm_resp_t res;
} mod_smpp_message_t;

typedef struct mod_smpp_message_multi_s {
  submit_multi_t      req;
  submit_multi_resp_t res;
} mod_smpp_message_multi_t;

#define MOD_SMPP_TRANS_RESP "mod_smpp::bind_transceiver_resp"

/* mod_smpp_gateway.c */
switch_status_t mod_smpp_gateway_authenticate(mod_smpp_gateway_t *gateway);
switch_status_t mod_smpp_gateway_connect(mod_smpp_gateway_t *gateway);
switch_status_t mod_smpp_gateway_create(mod_smpp_gateway_t **gw, char *name, char*host, int port, int debug, char *system_id, 
										char *password, char *system_type, char *profile);
switch_status_t mod_smpp_gateway_destroy(mod_smpp_gateway_t **gateway);
switch_status_t mod_smpp_gateway_send_message(mod_smpp_gateway_t *gateway, switch_event_t *message);
switch_status_t mod_smpp_gateway_send_multi(mod_smpp_gateway_t *gateway, switch_event_t *message);
switch_status_t mod_smpp_gateway_get_next_sequence(mod_smpp_gateway_t *gateway, uint32_t *seq);
switch_status_t mod_smpp_gateway_connection_read(mod_smpp_gateway_t *gateway, uint8_t *local_buffer,switch_size_t read_len, switch_event_t **event, unsigned int *command_id);
switch_status_t mod_smpp_gateway_send_unbind_response(mod_smpp_gateway_t *gateway,int seq_num);
switch_status_t mod_smpp_gateway_send_enquire_link(mod_smpp_gateway_t *gateway);
switch_status_t mod_smpp_gateway_send_enquire_link_response(mod_smpp_gateway_t *gateway,int seq_num);
switch_status_t mod_smpp_gateway_send_deliver_sm_response(mod_smpp_gateway_t *gateway, switch_event_t *event);
switch_status_t mod_smpp_gateway_send_query_sm(mod_smpp_gateway_t *gateway, char *source_addr,char *message_id);

/* mod_smpp_message.c */
switch_status_t mod_smpp_message_encode_body(char *body, int length, unsigned char *bin, uint8_t *enc_length);
switch_status_t mod_smpp_message_create(mod_smpp_gateway_t *gateway, switch_event_t *event, mod_smpp_message_t **message);
switch_status_t mod_smpp_message_create_multi(mod_smpp_gateway_t *gateway, switch_event_t *event, mod_smpp_message_multi_t **message);
switch_status_t mod_smpp_message_decode(mod_smpp_gateway_t *gateway, deliver_sm_t *res, switch_event_t **event);
switch_status_t mod_smpp_message_destroy(mod_smpp_message_t **msg);
switch_status_t mod_smpp_message_multi_destroy(mod_smpp_message_multi_t **msg);

/* mod_smpp_utils.c */
void mod_smpp_dump_pdu(void *pdu);

/* mod_smpp_data_coding.c */
int mod_smpp_utf8_to_gsm7_tb(unsigned char *utf8,unsigned char *gsm7);
int mod_smpp_gsm7_to_utf8_tb(unsigned char *gsm7,int gsm7_len,unsigned char *utf8);
int mod_smpp_utf8_to_gsm7(unsigned char *utf8,unsigned char *gsm7);
const char *mod_smpp_gsm7_to_utf8(unsigned char *gsm7,int gsm7_len);
int mod_smpp_data_coding_recognize(unsigned char *str_ptr);
int mod_smpp_utf8_to_ucs2 (unsigned char *input,unsigned char *output);
int mod_smpp_ucs2_to_utf8 (unsigned char *input, unsigned char *output);

/* mod_smpp_pgsql.c */


#endif /* MOD_SMPP_H */

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
