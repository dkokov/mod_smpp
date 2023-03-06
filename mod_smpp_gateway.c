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

#include <mod_smpp.h>

static void *SWITCH_THREAD_FUNC mod_smpp_gateway_read_thread(switch_thread_t *thread, void *obj);
static void *SWITCH_THREAD_FUNC mod_smpp_gateway_monitor_thread(switch_thread_t *thread, void *obj);

switch_status_t mod_smpp_gateway_create(mod_smpp_gateway_t **gw, char *name, char *host, int port, int debug, char *system_id, 
										char *password, char *system_type, char *profile) {
	mod_smpp_gateway_t *gateway = NULL;
	switch_memory_pool_t *pool = NULL;
	
  	switch_core_new_memory_pool(&pool);

	gateway = switch_core_alloc(pool, sizeof(mod_smpp_gateway_t));

	gateway->pool = pool;
	gateway->running = 1;
	gateway->sequence = 1;
	gateway->name = name ? switch_core_strdup(gateway->pool, name) : "default";
	gateway->host = host ? switch_core_strdup(gateway->pool, host) : "localhost";
	gateway->port = port ? port : 8000;
	gateway->debug = debug;
	gateway->system_id = system_id ? switch_core_strdup(gateway->pool, system_id) : "username";
	gateway->password = password ? switch_core_strdup(gateway->pool, password) : "password";
	gateway->system_type = system_type ? switch_core_strdup(gateway->pool, system_type) : "freeswitch_smpp";
	gateway->profile = profile ? switch_core_strdup(gateway->pool, profile) : "default";
	
	gateway->mon_thread_timer = 3;
	gateway->reconn_retry_sec = 3;

	if ( switch_sockaddr_info_get(&(gateway->socketaddr), gateway->host, SWITCH_INET, 
								  gateway->port, 0, mod_smpp_globals.pool) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get socketaddr info\n");
		goto err;
	}

	if ( switch_socket_create(&(gateway->socket), switch_sockaddr_get_family(gateway->socketaddr), 
							  SOCK_STREAM, SWITCH_PROTO_TCP, mod_smpp_globals.pool) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create the socket\n");
		goto err;
	}

	switch_mutex_init(&(gateway->conn_mutex), SWITCH_MUTEX_UNNESTED, mod_smpp_globals.pool);
	if (mod_smpp_gateway_connect(gateway) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to connect to gateway[%s]\n", gateway->name);
		goto err;
	}
	
	/* Start gateway read thread */
	switch_threadattr_create(&(gateway->thd_attr), mod_smpp_globals.pool);
	switch_threadattr_detach_set(gateway->thd_attr, 1);
	switch_threadattr_stacksize_set(gateway->thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&(gateway->thread), gateway->thd_attr, mod_smpp_gateway_read_thread, (void *) gateway, mod_smpp_globals.pool);
	
	/* Start gateway monitoring thread */
	switch_threadattr_create(&(gateway->mon_thd_attr), mod_smpp_globals.pool);
	switch_threadattr_detach_set(gateway->mon_thd_attr, 1);
	switch_threadattr_stacksize_set(gateway->mon_thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&(gateway->mon_thread), gateway->mon_thd_attr, mod_smpp_gateway_monitor_thread, (void *) gateway, mod_smpp_globals.pool);
	
	switch_mutex_init(&(gateway->mon_mutex), SWITCH_MUTEX_NESTED, mod_smpp_globals.pool);
	
	switch_core_hash_insert(mod_smpp_globals.gateways, name, (void *) gateway);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Gateway %s created\n", gateway->host);
	
	*gw = gateway;
	return SWITCH_STATUS_SUCCESS;
	
 err:
	mod_smpp_gateway_destroy(&gateway);
	return SWITCH_STATUS_GENERR;
}

switch_status_t mod_smpp_gateway_authenticate(mod_smpp_gateway_t *gateway) {
    bind_transceiver_t *req_b = calloc(sizeof(bind_transceiver_t), 1);
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_event_t *event = NULL;
    switch_size_t write_len = 0;
    unsigned int command_id = 0;
    uint8_t local_buffer[1024] = {0};

    switch_size_t read_len = 1024;

    int local_buffer_len = 1024;

    req_b->command_length   = 0;
    req_b->command_id       = BIND_TRANSCEIVER;
    req_b->command_status   = ESME_ROK;
    req_b->addr_npi         = 1;
    req_b->addr_ton         = 1;

    strncpy( (char *)req_b->address_range, gateway->host, sizeof(req_b->address_range));

    if ( gateway->system_id ) {
		strncpy((char *)req_b->system_id, gateway->system_id, sizeof(req_b->system_id));
    }

    if ( gateway->password ) {
		strncpy((char *)req_b->password, gateway->password, sizeof(req_b->password));
    }

    if ( gateway->system_type ) {
		strncpy((char *)req_b->system_type, gateway->system_type, sizeof(req_b->system_type));
    }

    req_b->interface_version = SMPP_VERSION;

    mod_smpp_gateway_get_next_sequence(gateway, &(req_b->sequence_number));

    // Not thread safe due to smpp34_errno and smpp34_strerror since they are global to running process.
    // The risk here is that the errno and strerror variables will be corrupted due to race conditions if there are errors
    if ( smpp34_pack2( local_buffer, sizeof(local_buffer), &local_buffer_len, (void*)req_b) != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error in smpp_pack():%d:\n%s\n", smpp34_errno, smpp34_strerror);
		status = SWITCH_STATUS_GENERR;
		goto done;
    }

    write_len = local_buffer_len;

    if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) req_b);
    }

    if ( switch_socket_send(gateway->socket, (char *) local_buffer, &write_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send on the socket\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    if( write_len != local_buffer_len ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Was not able to send entire buffer\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    memset(local_buffer,0,1024);

    if ( ( switch_socket_recv(gateway->socket, (char *) local_buffer, &read_len)) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to recv on the socket\n");
    }

    if(local_buffer != NULL) mod_smpp_gateway_connection_read(gateway, local_buffer , read_len, &event, &command_id);

 done:
    if ( (mod_smpp_globals.debug || gateway->debug ) && event ) {
		char *str = NULL;
		switch_event_serialize(event, &str, SWITCH_TRUE);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Auth response: %s\n", str);
		switch_safe_free(str);
    }

    if ( req_b ) {
		switch_safe_free(req_b);
    }

    return status;
}

/* When connecting to a gateway, the first message must be an authentication attempt */
switch_status_t mod_smpp_gateway_connect(mod_smpp_gateway_t *gateway) 
{
    switch_status_t status;

    if ( (status = switch_socket_connect(gateway->socket, gateway->socketaddr)) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to connect the socket %d\n", status);
		return SWITCH_STATUS_GENERR;
    }

    if ( mod_smpp_gateway_authenticate(gateway) != SWITCH_STATUS_SUCCESS ) {
		return SWITCH_STATUS_GENERR;
    }

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t mod_smpp_message_decode_submit_sm_resp(mod_smpp_gateway_t *gateway,submit_sm_resp_t *res,switch_event_t **event)
{
	switch_event_t *evt = NULL;
	char *str = NULL;

	if (switch_event_create(&evt, SWITCH_EVENT_MESSAGE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create new event\n"); 
	}

	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "endpoint", "mod_smpp");

	str = switch_mprintf("%d", res->sequence_number);
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "sequence_number", str);

	str = switch_mprintf("%s", res->message_id);
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "message_id", str);

	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "smpp_gateway", gateway->name);
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "proto", "smpp");
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "profile", gateway->profile);

	*event = evt;

	return SWITCH_STATUS_SUCCESS;
}

/* Expects the gateway to be locked already */
switch_status_t mod_smpp_gateway_connection_read(mod_smpp_gateway_t *gateway,uint8_t *local_buffer,switch_size_t read_len, switch_event_t **event, unsigned int *command_id)
{
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_event_t *evt = NULL;
    generic_nack_t *gennack = NULL;
    char data[2048] = {0}; /* local buffer for unpacked PDU */

    if ( smpp34_unpack2((void *)data, local_buffer, read_len) ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,  "smpp: error decoding the receive buffer:%d:%s\n", smpp34_errno, smpp34_strerror);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) data);
    }

    gennack = (generic_nack_t *) data;
    *command_id = gennack->command_id;
    switch(*command_id) {
    case BIND_TRANSCEIVER_RESP:
		if ( (gennack->command_status != ESME_ROK) ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Authentication Failure %d\n", gennack->command_status);
		}
		break;
    case UNBIND:
		mod_smpp_gateway_send_unbind_response(gateway,gennack->sequence_number);
		break;
    case QUERY_SM:
		break;
    case QUERY_SM_RESP:
		if ( gennack->command_status == ESME_ROK ) {
		    char sql[1024] = {0};
		
		    query_sm_resp_t *res = (query_sm_resp_t *) data;
		
		    sprintf(sql,"update message_scheduler set dr_recv_ts = %d,dr_net_error_code = %d,final_date = '%s',dr_message_state = %d "
				"where receipted_msg_id = '%s'",
				(int)time(NULL),res->error_code,res->final_date,res->message_state,res->message_id);
				
		    switch_mutex_lock(mod_smpp_globals.db_mutex);
		
		    if(switch_pgsql_send_query(mod_smpp_globals.pgsql,sql) == SWITCH_PGSQL_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "sql: %s\n",sql);
		    }
		
		    switch_pgsql_flush(mod_smpp_globals.pgsql);
		
		    switch_mutex_unlock(mod_smpp_globals.db_mutex);
		}
		break;
    case DELIVER_SM:
		if ( gennack->command_status == ESME_ROK ) {
		    deliver_sm_t *res = (deliver_sm_t *) data;
		
		    mod_smpp_message_decode(gateway, res, &evt);
		}
		break;
    case ENQUIRE_LINK:
		if(gennack->command_status == ESME_ROK) {
			enquire_link_resp_t *res = (enquire_link_resp_t *) data;
			
			switch_mutex_lock(gateway->mon_mutex);
			gateway->last_recv = time(NULL);
			switch_mutex_unlock(gateway->mon_mutex);
			
			mod_smpp_gateway_send_enquire_link_response(gateway,res->sequence_number);
		}
		break;
    case ENQUIRE_LINK_RESP:
		break;
    case SUBMIT_SM_RESP:
		if(gennack->command_status == ESME_ROK) {
			submit_sm_resp_t *res = (submit_sm_resp_t *) data;
			
			mod_smpp_message_decode_submit_sm_resp(gateway, res, &evt);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SUBMIT_SM_RESP command_status: %d\n",gennack->command_status);
		}
		
		break;
    case SUBMIT_MULTI_RESP:
		break;
    default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unrecognized Command ID: %u\n", *command_id);
    }

    /* Only need to create/expand the event if it is one of the PDU's that we care about */
    if (evt) {
		*event = evt;
    }

 done: 
    return status;
}

static void *SWITCH_THREAD_FUNC mod_smpp_gateway_read_thread(switch_thread_t *thread, void *obj)
{
//    int i;

    mod_smpp_gateway_t *gateway = (mod_smpp_gateway_t *) obj;

    switch_size_t smpp_pdu_len = 0;
    switch_size_t read_len ;
    uint8_t *local_buffer = calloc(BUFSZ, 1);
    uint32_t *local_buffer32 ;

    switch_event_t *event;
    unsigned int command_id;

    while ( gateway->running ) {
		read_len = BUFSZ;
		
		if ( (switch_socket_recv(gateway->socket, (char *) local_buffer, &read_len)) != SWITCH_STATUS_SUCCESS ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_socket_recv()!!! ERROR !!! \n");
			
			switch_socket_close(gateway->socket);
			
			if ( mod_smpp_gateway_connect(gateway) != SWITCH_STATUS_SUCCESS ) {
			    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "The gateway [gw:%s] is not reconnected successful!\n",gateway->name);
			} else {
			    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "The gateway [gw:%s] is reconnected successful!\n",gateway->name);
			}
			
			/* Retry after 3sec */
			switch_sleep((gateway->reconn_retry_sec*1000000));
			
			continue;
		}
		
		again:
		event = NULL;
		command_id = 0;
		local_buffer32 = (uint32_t *) local_buffer;
		smpp_pdu_len = ntohl(*local_buffer32 );
		
		if ( ( local_buffer != NULL ) && ( read_len > 0 ) && ( smpp_pdu_len > 0 )) {
		    mod_smpp_gateway_connection_read(gateway, local_buffer, smpp_pdu_len, &event, &command_id);
		}
		
		if ( command_id == 0 ) continue;
		
		if ( (mod_smpp_globals.debug || gateway->debug) && event) {
			char *str = NULL;
			switch_event_serialize(event, &str, 0);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Received message event [%d]: \n%s\n\n", command_id,str);
			switch_safe_free(str);
		}

	    switch(command_id) {
	    case BIND_TRANSCEIVER_RESP:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Why did we get an unexpected authentication response?\n");
			break;
	    case UNBIND:
			switch_socket_close(gateway->socket);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "The socket [gw:%s] is closed!\n",gateway->name);
			break;
	    case ENQUIRE_LINK:
			break;
	    case DELIVER_SM:
			mod_smpp_gateway_send_deliver_sm_response(gateway, event);
			
			switch_core_chat_send("sip", event);
			
			switch_event_destroy(&event);
			/* Fire message to the chat plan, then respond */
			
			break;
	    case QUERY_SM_RESP:
			break;
	    case SUBMIT_SM_RESP:
			if ( event ) {
				char sql[1024] = {0};
				
				sprintf(sql,"update message_scheduler set receipted_msg_id = '%s',msg_recv_sm_resp_ts = %d,msg_state_id = 2 "
					    "where transaction_id = %d and sip_smpp_gw_uuid = '%s'",
					    switch_event_get_header(event,"message_id"),(int)time(NULL),
					    atoi(switch_event_get_header(event,"sequence_number")),
					    gateway->sip_smpp_gw_uuid);
				
				switch_mutex_lock(mod_smpp_globals.db_mutex);
				
				if(switch_pgsql_send_query(mod_smpp_globals.pgsql,sql) == SWITCH_PGSQL_SUCCESS) {
				    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "sql: %s\n",sql);
				}
				
				switch_pgsql_flush(mod_smpp_globals.pgsql);
				
				switch_mutex_unlock(mod_smpp_globals.db_mutex);
				
				switch_event_destroy(&event);
			}
			break;
	    case SUBMIT_MULTI_RESP:
			break;
	    case ENQUIRE_LINK_RESP:
			switch_mutex_lock(gateway->mon_mutex);
			gateway->last_recv = time(NULL);
			switch_mutex_unlock(gateway->mon_mutex);
			
			break;
	    default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown command_id[%d]\n", command_id);
			if ( event ) {
				char *str = NULL;
				switch_event_serialize(event, &str, SWITCH_FALSE);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown packet event: %s\n", str);
				switch_safe_free(str);
			}
		}
		
		if(read_len > smpp_pdu_len) {
		    local_buffer = &local_buffer[smpp_pdu_len];
		    read_len = read_len - smpp_pdu_len;
		    goto again;
		} else if(read_len == smpp_pdu_len) {
		    local_buffer = &local_buffer[0];
		}
    }

    return 0;
}

static void *SWITCH_THREAD_FUNC mod_smpp_gateway_monitor_thread(switch_thread_t *thread, void *obj)
{
    time_t cur;
    mod_smpp_gateway_t *gateway = (mod_smpp_gateway_t *) obj;

    while(gateway->running) {
	cur = time(NULL);
	
	if((cur - gateway->last_recv) > gateway->mon_ts) {
	    if(mod_smpp_gateway_send_enquire_link(gateway) == SWITCH_STATUS_SUCCESS) {
		switch_mutex_lock(gateway->mon_mutex);
		gateway->last_send = cur;
		switch_mutex_unlock(gateway->mon_mutex);
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Sent ENQUIRE_LINK to '%s' gateway\n",gateway->name);
		
	    } else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send ENQUIRE_LINK to '%s' gateway\n",gateway->name);
	    }
	}
	
//	if(cur - gateway->last_recv >= 30) {
//	    switch_socket_close(gateway->socket);
//		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DKOKOV MONITOR'%s' gateway\n",gateway->name);
//	}
	
	switch_sleep((gateway->mon_thread_timer*1000000));
    }

    return 0;
}

switch_status_t mod_smpp_gateway_send_enquire_link(mod_smpp_gateway_t *gateway)
{
    enquire_link_t *enquire = calloc(sizeof(enquire_link_t), 1);
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_size_t write_len = 0;
    uint8_t local_buffer[128] = {0};
    int local_buffer_len = sizeof(local_buffer);

    enquire->command_length   = 0;
    enquire->command_id       = ENQUIRE_LINK;
    enquire->command_status   = ESME_ROK;

    mod_smpp_gateway_get_next_sequence(gateway, &(enquire->sequence_number));

    if ( smpp34_pack2( local_buffer, sizeof(local_buffer), &local_buffer_len, (void*)enquire) != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error in smpp_pack():%d:\n%s\n", smpp34_errno, smpp34_strerror);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    write_len = local_buffer_len;

    if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) enquire);
    }

    if ( switch_socket_send(gateway->socket, (char *) local_buffer, &write_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send on the socket\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    if( write_len != local_buffer_len ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Was not able to send entire buffer\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

 done:
    switch_safe_free(enquire);

    return status;
}

switch_status_t mod_smpp_gateway_send_enquire_link_response(mod_smpp_gateway_t *gateway,int seq_num)
{
    enquire_link_resp_t *enquire_resp = calloc(sizeof(enquire_link_resp_t), 1);
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_size_t write_len = 0;
    uint8_t local_buffer[128] = {0};
    int local_buffer_len = sizeof(local_buffer);

    enquire_resp->command_length   = 0;
    enquire_resp->command_id       = ENQUIRE_LINK_RESP;
    enquire_resp->command_status   = ESME_ROK;
    enquire_resp->sequence_number = seq_num;

    if ( smpp34_pack2( local_buffer, sizeof(local_buffer), &local_buffer_len, (void*)enquire_resp) != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error in smpp_pack():%d:\n%s\n", smpp34_errno, smpp34_strerror);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    write_len = local_buffer_len;

    if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) enquire_resp);
    }

    if ( switch_socket_send(gateway->socket, (char *) local_buffer, &write_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send on the socket\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    if( write_len != local_buffer_len ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Was not able to send entire buffer\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

 done:
    switch_safe_free(enquire_resp);

    return status;
}

switch_status_t mod_smpp_gateway_send_unbind_response(mod_smpp_gateway_t *gateway,int seq_num)
{
    unbind_resp_t *unbind_resp = calloc(sizeof(unbind_resp_t), 1);
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_size_t write_len = 0;
    uint8_t local_buffer[128] = {0};
    int local_buffer_len = sizeof(local_buffer);

    unbind_resp->command_length   = 0;
    unbind_resp->command_id       = UNBIND_RESP;
    unbind_resp->command_status   = ESME_ROK;
    unbind_resp->sequence_number  = seq_num;

    if ( smpp34_pack2( local_buffer, sizeof(local_buffer), &local_buffer_len, (void*)unbind_resp) != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error in smpp_pack():%d:\n%s\n", smpp34_errno, smpp34_strerror);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    write_len = local_buffer_len;

    if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) unbind_resp);
    }

    if ( switch_socket_send(gateway->socket, (char *) local_buffer, &write_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send on the socket\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    if( write_len != local_buffer_len ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Was not able to send entire buffer\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

 done:
    switch_safe_free(unbind_resp);

    return status;
}

switch_status_t mod_smpp_gateway_send_deliver_sm_response(mod_smpp_gateway_t *gateway, switch_event_t *event)
{
    deliver_sm_resp_t *deliver_sm_resp = calloc(sizeof(deliver_sm_resp_t), 1);
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_size_t write_len = 0;
    uint8_t local_buffer[128] = {0};
    int local_buffer_len = sizeof(local_buffer);

    deliver_sm_resp->command_length   = 0;
    deliver_sm_resp->command_id       = DELIVER_SM_RESP;
    deliver_sm_resp->command_status   = ESME_ROK;

    deliver_sm_resp->sequence_number  = atoi(switch_event_get_header(event, "sequence_number"));

    if ( smpp34_pack2( local_buffer, sizeof(local_buffer), &local_buffer_len, (void*)deliver_sm_resp) != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error in smpp_pack():%d:\n%s\n", smpp34_errno, smpp34_strerror);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    write_len = local_buffer_len;

    if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) deliver_sm_resp);
    }

    if ( switch_socket_send(gateway->socket, (char *) local_buffer, &write_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send on the socket\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    if( write_len != local_buffer_len ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Was not able to send entire buffer\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

 done:
    switch_safe_free(deliver_sm_resp);
    return status;
}

switch_status_t mod_smpp_gateway_send_unbind(mod_smpp_gateway_t *gateway)
{
    unbind_t *unbind_req = calloc(sizeof(unbind_t), 1);
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_size_t write_len = 0;
    uint8_t local_buffer[128] = {0};
    int local_buffer_len = sizeof(local_buffer);

    unbind_req->command_length   = 0;
    unbind_req->command_id       = UNBIND;
    unbind_req->command_status   = ESME_ROK;

    mod_smpp_gateway_get_next_sequence(gateway, &(unbind_req->sequence_number));

    if ( smpp34_pack2( local_buffer, sizeof(local_buffer), &local_buffer_len, (void*)unbind_req) != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error in smpp_pack():%d:\n%s\n", smpp34_errno, smpp34_strerror);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	write_len = local_buffer_len;

	if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) unbind_req);
	}

	if ( switch_socket_send(gateway->socket, (char *) local_buffer, &write_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send on the socket\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

    if( write_len != local_buffer_len ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Was not able to send entire buffer\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

 done:
	switch_safe_free(unbind_req);

	return status;
}

switch_status_t mod_smpp_gateway_send_query_sm(mod_smpp_gateway_t *gateway, char *source_addr,char *message_id) {
    query_sm_t *query = calloc(sizeof(query_sm_t), 1);
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_size_t write_len = 0;
    uint8_t local_buffer[1024] = {0};
    int local_buffer_len = 1024;

    query->command_length   = 0;
    query->command_id       = QUERY_SM;
    query->command_status   = ESME_ROK;

    mod_smpp_gateway_get_next_sequence(gateway, &(query->sequence_number));

    query->source_addr_npi  = 1;
    query->source_addr_ton  = 1;

    strncpy( (char *)query->source_addr, source_addr, sizeof(query->source_addr));
    strncpy( (char *)query->message_id, message_id, sizeof(query->message_id));

    if ( smpp34_pack2( local_buffer, sizeof(local_buffer), &local_buffer_len, (void*)query) != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error in smpp_pack():%d:\n%s\n", smpp34_errno, smpp34_strerror);
		status = SWITCH_STATUS_GENERR;
		goto done;
    }

    write_len = local_buffer_len;

    if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) query);
    }

    if ( switch_socket_send(gateway->socket, (char *) local_buffer, &write_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send on the socket\n");
    }

    if( write_len != local_buffer_len ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Was not able to send entire buffer\n");
    }

done:
    if ( query ) {
		switch_safe_free(query);
    }

    return status;
}

switch_status_t mod_smpp_gateway_destroy(mod_smpp_gateway_t **gw) 
{
	mod_smpp_gateway_t *gateway = NULL;

	if ( !gw || !*gw ) {
		return SWITCH_STATUS_SUCCESS;
	}
	
	gateway = *gw;
	
	switch_core_hash_delete(mod_smpp_globals.gateways, gateway->name);

	gateway->running = 0;

	mod_smpp_gateway_send_unbind(gateway);

	switch_socket_shutdown(gateway->socket, SWITCH_SHUTDOWN_READWRITE);
	switch_socket_close(gateway->socket);

	switch_core_destroy_memory_pool(&(gateway->pool));
	switch_mutex_destroy(gateway->conn_mutex);

	*gw = NULL;
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t mod_smpp_gateway_get_next_sequence(mod_smpp_gateway_t *gateway, uint32_t *seq)
{

	switch_mutex_lock(gateway->conn_mutex);
	gateway->sequence++;
	*seq = gateway->sequence;
	switch_mutex_unlock(gateway->conn_mutex);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t mod_smpp_gateway_send_message(mod_smpp_gateway_t *gateway, switch_event_t *message) {
    mod_smpp_message_t *msg = NULL;
    uint8_t local_buffer[1024];
    int local_buffer_len = sizeof(local_buffer);
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_size_t write_len = 0;

    if ( mod_smpp_message_create(gateway, message, &msg) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to send message due to message_create failure\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    memset(local_buffer, 0, sizeof(local_buffer));

    if( smpp34_pack2( local_buffer, sizeof(local_buffer),&local_buffer_len, (void*)&(msg->req)) != 0 ){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Unable to encode message:%d:\n%s\n", smpp34_errno, smpp34_strerror); 
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    write_len = local_buffer_len;

    if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) &(msg->req));
    }

    if ( switch_socket_send(gateway->socket, (char *) local_buffer, &write_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send on the socket\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    if ( mod_smpp_gateway_get_next_sequence(gateway, &(msg->req.sequence_number)) ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to fetch next gateway sequence number\n"); 
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    if ( write_len != local_buffer_len ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Did not send all of message to gateway");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

 done:
    mod_smpp_message_destroy(&msg);
    return status;
}

switch_status_t mod_smpp_gateway_send_multi(mod_smpp_gateway_t *gateway, switch_event_t *message) {
    mod_smpp_message_multi_t *msg = NULL;
    uint8_t local_buffer[1024];
    int local_buffer_len = sizeof(local_buffer);
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_size_t write_len = 0;

    if ( mod_smpp_message_create_multi(gateway, message, &msg) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to send message due to message_create failure\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    memset(local_buffer, 0, sizeof(local_buffer));

    if( smpp34_pack2( local_buffer, sizeof(local_buffer),&local_buffer_len, (void*)&(msg->req)) != 0 ){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Unable to encode message:%d:\n%s\n", smpp34_errno, smpp34_strerror); 
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    write_len = local_buffer_len;

    if ( mod_smpp_globals.debug || gateway->debug ) {
		mod_smpp_dump_pdu((void *) &(msg->req));
    }

    if ( switch_socket_send(gateway->socket, (char *) local_buffer, &write_len) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send on the socket\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    if ( mod_smpp_gateway_get_next_sequence(gateway, &(msg->req.sequence_number)) ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to fetch next gateway sequence number\n"); 
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    if ( write_len != local_buffer_len ){ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "smpp: Did not send all of message to gateway");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

 done:
    mod_smpp_message_multi_destroy(&msg);
    return status;
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
