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

switch_status_t mod_smpp_message_encode_body(char *body, int length, unsigned char *bin, uint8_t *enc_length)
{
    int i = 0;

    for ( i = 0; i < length; i++ ) {
		bin[i*2] = body[i] / 16;
		bin[i*2 + 1] = body[i] % 16;
    }

    *enc_length = (i * 2) + 1;
    return SWITCH_STATUS_SUCCESS;
}

/* 
   Scratch notes taken during development/interop:

	char *message = "5361792048656c6c6f20746f204d79204c6974746c6520467269656e64";
	''.join('%02x' % ord(c) for c in  u'Скажите привет моему маленькому другу'.encode('utf16'))

	Variable length UTF-16 russian text: 

	char *message = "fffe21043a043004360438044204350420003f044004380432043504420420003c043e0435043c04430420003c0430043b0435043d044c043a043e043c044304200034044004430433044304";
	char *mesg_txt = "This is a test SMS message from FreeSWITCH over SMPP";
	*/

switch_status_t mod_smpp_message_create(mod_smpp_gateway_t *gateway, switch_event_t *event, mod_smpp_message_t **message)
{
    char sql[1024] = {0};
    char *str = NULL;
    char *tmp = NULL;
    tlv_t tlv;

    int sar_total = 0;
    int sar_ref   = 0;
    int sar_num   = 0;

    int body_len = 0;
    int ucs2_len = 0;
    int gsm7_len = 0;

    int msg_num_retry = 0;

    int remote_msg_id;
    int validity;
    int smsapi_last_ts;
    char smsapi_from[5] = {0};

    unsigned char output[255] = {0};
    char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];

    mod_smpp_message_t *msg = calloc(1, sizeof(mod_smpp_message_t));
    char *body = switch_event_get_body(event);

    switch_uuid_str(uuid_str, sizeof(uuid_str));

    assert(*message == NULL);

    if ( !body ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to encode message missing body\n");
		goto err;
    }

    if ( mod_smpp_globals.debug || gateway->debug ) {
		char *str = NULL;
		switch_event_serialize(event, &str, 0);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Creating message from event:\n%s\n", str);
		switch_safe_free(str);
    }

    msg->req.command_id              = SUBMIT_SM;
    msg->req.command_status          = ESME_ROK;
    msg->req.command_length          = 0;
    msg->req.protocol_id             = 0;
    msg->req.priority_flag           = 0;
    msg->req.replace_if_present_flag = 0;
    msg->req.sm_default_msg_id       = 0;

    str = switch_event_get_header(event, "source_addr_ton");
    if( str == NULL ) msg->req.source_addr_ton = 1;
    else msg->req.source_addr_ton = atoi(str);

    str = switch_event_get_header(event, "source_addr_npi");
    if( str == NULL ) msg->req.source_addr_npi = 1;
    else msg->req.source_addr_npi = atoi(str);

    str = switch_event_get_header(event, "dest_addr_ton");
    if( str == NULL ) msg->req.dest_addr_ton = 1;
    else msg->req.dest_addr_ton = atoi(str);

    str = switch_event_get_header(event, "dest_addr_npi");
    if( str == NULL ) msg->req.dest_addr_npi = 1;
    else msg->req.dest_addr_npi = atoi(str);

    mod_smpp_gateway_get_next_sequence(gateway, &(msg->req.sequence_number));

    str = switch_event_get_header(event, "registered_delivery");
    if( str == NULL ) msg->req.registered_delivery = 0;
    else msg->req.registered_delivery = atoi(str);

    str = switch_event_get_header(event, "esm_class");
    if ( str == NULL ) msg->req.esm_class = 1;
    else msg->req.esm_class = atoi(str);

    str = switch_event_get_header(event, "schedule_delivery_time");
    if ( str == NULL ) tmp = "";
    else tmp = str;

    snprintf((char *)msg->req.schedule_delivery_time, sizeof(msg->req.schedule_delivery_time), "%s", tmp);

/* remote_msg_id smsapi_last_ts validity */
    str = switch_event_get_header(event, "remote_msg_id");
    if(str == NULL) remote_msg_id = -1;
    else remote_msg_id = atoi(str);

    str = switch_event_get_header(event, "smsapi_last_ts");
    if(str == NULL) smsapi_last_ts = -1;
    else smsapi_last_ts = atoi(str);

    str = switch_event_get_header(event, "smsapi_from");
    if(str != NULL) strcpy(smsapi_from,str);
//    else smsapi_from = str;

    str = switch_event_get_header(event, "validity_period");
    if(str == NULL) tmp = "";
    else tmp = str;

    snprintf((char *)msg->req.validity_period, sizeof(msg->req.validity_period), "%s", tmp);

    snprintf((char *)msg->req.service_type, sizeof(msg->req.service_type), "%s", "SMS");
    snprintf((char *)msg->req.source_addr, sizeof(msg->req.source_addr), "%s", switch_event_get_header(event, "from_user"));
    snprintf((char *)msg->req.destination_addr, sizeof(msg->req.destination_addr), "%s", switch_event_get_header(event, "to_user"));

    /* TLVs encoding: */
    memset(&tlv, 0, sizeof(tlv_t));

    str = switch_event_get_header(event, "sar_total_segments");
    if( str != NULL ) {
		sar_total = atoi(str);
		tlv.tag = TLVID_sar_total_segments;
		tlv.length = sizeof(uint8_t);
		tlv.value.octet[0] = sar_total;
		build_tlv( &(msg->req.tlv), &tlv );
    }

    str = switch_event_get_header(event, "sar_segment_seqnum");
    if( str != NULL ) {
		sar_num = atoi(str);
		tlv.tag = TLVID_sar_segment_seqnum;
		tlv.length = sizeof(uint8_t);
		tlv.value.octet[0] = sar_num;
		build_tlv( &(msg->req.tlv), &tlv );
    }

    str = switch_event_get_header(event, "sar_msg_ref_num");
    if( str != NULL ) {
		sar_ref = atoi(str);
		tlv.tag = TLVID_sar_msg_ref_num;
		tlv.length = sizeof(uint16_t);
		tlv.value.val16 = sar_ref;
		build_tlv( &(msg->req.tlv), &tlv );
    }

    str = switch_event_get_header(event, "msg_num_retry");
    if ( str != NULL ) msg_num_retry = atoi(str);
    else msg_num_retry = 0;

//    if( gateway->data_coding_recognize ) {
//		msg->req.data_coding = mod_smpp_data_coding_recognize((unsigned char *)body);
//    } else {
		str = switch_event_get_header(event, "data_coding");
	
		if( str == NULL ) msg->req.data_coding = 0;
		else msg->req.data_coding = atoi(str);
//    }

    sprintf(sql,"insert into message_scheduler "
	    "(sms_type,orig_msisdn,dest_msisdn,transaction_id,msg_send_sm_ts,short_msg,dest_smpp_gw,msg_state_id,msg_num_retry,sip_smpp_gw_uuid,"
	    "sar_msg_ref_num,sar_total_segments,sar_segment_seqnum,msg_sched_last_ts,smpp_msg_uuid,registered_delivery,sip_domain,remote_msg_id,"
	    "smsapi_last_ts,smsapi_from)"
	    "values ('smpp','%s','%s',%d,%d,'%s','%s',1,%d,'%s',%d,%d,%d,%d,'%s',%d,'%s',%d,%d,'%s')",
    msg->req.source_addr,msg->req.destination_addr,msg->req.sequence_number,(int)time(NULL),body,gateway->name,
    msg_num_retry,gateway->sip_smpp_gw_uuid,sar_ref,sar_total,sar_num,(int)time(NULL),uuid_str,msg->req.registered_delivery,
    switch_event_get_header(event, "sip_domain"),remote_msg_id,smsapi_last_ts,smsapi_from);

    switch_mutex_lock(mod_smpp_globals.db_mutex);

    if(switch_pgsql_send_query(mod_smpp_globals.pgsql,sql) == SWITCH_PGSQL_SUCCESS) {
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "insert: %s\n",sql);
    }

    switch_pgsql_flush(mod_smpp_globals.pgsql);

    switch_mutex_unlock(mod_smpp_globals.db_mutex);

    if( msg->req.data_coding == 8) {
	ucs2_len = mod_smpp_utf8_to_ucs2((unsigned char *)body,output);
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ucs2_len: %d\n",ucs2_len);
	
	if( ucs2_len <= 0) {
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ucs2_len: %d\n",ucs2_len);
	    goto err;
	}
	
	body_len = ucs2_len;
	msg->req.sm_length = ucs2_len;
	memcpy(msg->req.short_message,output,ucs2_len);
    } else if ( msg->req.data_coding == 3) {
	body_len = strlen(body);
	msg->req.sm_length = body_len;
	snprintf((char *)msg->req.short_message, sizeof(msg->req.short_message), "%s", body);
    } else if ( msg->req.data_coding == 4) {
	
    } else if ( msg->req.data_coding == 0) {
	gsm7_len = mod_smpp_utf8_to_gsm7((unsigned char *)body,output);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "gsm7_len: %d\n",gsm7_len);
	
	body_len = gsm7_len;
	msg->req.sm_length = gsm7_len;
	memcpy(msg->req.short_message,output,gsm7_len);
    }

    *message = msg;
    return SWITCH_STATUS_SUCCESS;

 err:
    switch_safe_free(msg);
    return SWITCH_STATUS_GENERR;
}

switch_status_t mod_smpp_message_create_multi(mod_smpp_gateway_t *gateway, switch_event_t *event, mod_smpp_message_multi_t **message)
{
    char *str = NULL;
    char *tmp = NULL;
    tlv_t tlv;
    dad_t dad;

    int ucs2_len = 0;
    unsigned char output[255] = {0};

    mod_smpp_message_multi_t *msg = calloc(1, sizeof(mod_smpp_message_multi_t));
    char *body = switch_event_get_body(event);

    assert(*message == NULL);

    if ( !body ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to encode message missing body\n");
		goto err;
    }

    if ( mod_smpp_globals.debug || gateway->debug ) {
		char *str = NULL;
		switch_event_serialize(event, &str, 0);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Creating message from event:\n%s\n", str);
		switch_safe_free(str);
    }

    msg->req.command_id              = SUBMIT_MULTI;
    msg->req.command_status          = ESME_ROK;
    msg->req.command_length          = 0;
    msg->req.protocol_id             = 0;
    msg->req.priority_flag           = 0;
    msg->req.replace_if_present_flag = 0;
    msg->req.sm_default_msg_id       = 0;
    msg->req.source_addr_ton         = 1;
    msg->req.source_addr_npi         = 1;

    msg->req.number_of_dests         = 2;

    memset(&dad, 0, sizeof(dad_t));

    dad.dest_flag = DFID_SME_Address;
    dad.value.sme.dest_addr_ton = 1;
    dad.value.sme.dest_addr_npi = 1;
    snprintf((char*)dad.value.sme.destination_addr,sizeof(dad.value.sme.destination_addr), "%s", "359882079235");
    build_dad( &(msg->req.dest_addr_def), &dad );

    dad.dest_flag = DFID_SME_Address;
    dad.value.sme.dest_addr_ton = 1;
    dad.value.sme.dest_addr_npi = 1;
    snprintf((char*)dad.value.sme.destination_addr,sizeof(dad.value.sme.destination_addr), "%s", "359882079242");
    build_dad( &(msg->req.dest_addr_def), &dad );
/*
    dad.dest_flag = DFID_SME_Address;
    dad.value.sme.dest_addr_ton = 1;
    dad.value.sme.dest_addr_npi = 1;
    snprintf((char*)dad.value.sme.destination_addr,sizeof(dad.value.sme.destination_addr), "%s", "359892224569");
    build_dad( &(msg->req.dest_addr_def), &dad );

    dad.dest_flag = DFID_SME_Address;
    dad.value.sme.dest_addr_ton = 1;
    dad.value.sme.dest_addr_npi = 1;
    snprintf((char*)dad.value.sme.destination_addr,sizeof(dad.value.sme.destination_addr), "%s", "359892224568");
    build_dad( &(msg->req.dest_addr_def), &dad );
*/
    mod_smpp_gateway_get_next_sequence(gateway, &(msg->req.sequence_number));

    str = switch_event_get_header(event, "registered_delivery");
    if( str == NULL ) msg->req.registered_delivery = 0;
    else msg->req.registered_delivery = atoi(str);

    str = switch_event_get_header(event, "esm_class");
    if ( str == NULL ) msg->req.esm_class = 1;
    else msg->req.esm_class = atoi(str);

    str = switch_event_get_header(event, "schedule_delivery_time");
    if ( str == NULL ) tmp = "";
    else tmp = str;

    snprintf((char *)msg->req.schedule_delivery_time, sizeof(msg->req.schedule_delivery_time), "%s", tmp);

    str = switch_event_get_header(event, "validity_period");
    if(str == NULL) tmp = "";
    else tmp = str;

    snprintf((char *)msg->req.validity_period, sizeof(msg->req.validity_period), "%s", tmp);

    snprintf((char *)msg->req.service_type, sizeof(msg->req.service_type), "%s", "SMS");
    snprintf((char *)msg->req.source_addr, sizeof(msg->req.source_addr), "%s", switch_event_get_header(event, "from_user"));

    /* TLVs encoding: */
    memset(&tlv, 0, sizeof(tlv_t));

    str = switch_event_get_header(event, "sar_total_segments");
    if( str != NULL ) {
		tlv.tag = TLVID_sar_total_segments;
		tlv.length = sizeof(uint8_t);
		tlv.value.octet[0] = atoi(str);
		build_tlv( &(msg->req.tlv), &tlv );
    }

    str = switch_event_get_header(event, "sar_segment_seqnum");
    if( str != NULL ) {
		tlv.tag = TLVID_sar_segment_seqnum;
		tlv.length = sizeof(uint8_t);
		tlv.value.octet[0] = atoi(str);
		build_tlv( &(msg->req.tlv), &tlv );
    }

    str = switch_event_get_header(event, "sar_msg_ref_num");
    if( str != NULL ) {
		tlv.tag = TLVID_sar_msg_ref_num;
		tlv.length = sizeof(uint16_t);
		tlv.value.val16 = atoi(str);
		build_tlv( &(msg->req.tlv), &tlv );
    }

    if( gateway->data_coding_recognize ) {
		/* Automatic recognize , UTF-8(data_coding 0) or UCS-2(data_coding 8) */
		msg->req.data_coding = mod_smpp_data_coding_recognize((unsigned char *)body);
    } else {
		str = switch_event_get_header(event, "data_coding");
	
		if( str == NULL ) msg->req.data_coding = 0;
		else msg->req.data_coding = atoi(str);
    }

    if( msg->req.data_coding ) {
		ucs2_len = mod_smpp_utf8_to_ucs2((unsigned char *)body,output);
		memcpy(body,output,ucs2_len);
    }

    if (ucs2_len == 0) {
		msg->req.sm_length = strlen(body);
		snprintf((char *)msg->req.short_message, sizeof(msg->req.short_message), "%s", body);
    } else {
		msg->req.sm_length = ucs2_len;
		memcpy(msg->req.short_message,body,ucs2_len);
    }

    if ( 0 && mod_smpp_message_encode_body(body, strlen(body), 
       (unsigned char *) &(msg->req.short_message), &(msg->req.sm_length)) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to encode message body\n");
		goto err;
    }

    *message = msg;
    return SWITCH_STATUS_SUCCESS;

 err:
    switch_safe_free(msg);
    return SWITCH_STATUS_GENERR;
}

switch_status_t mod_smpp_message_decode(mod_smpp_gateway_t *gateway, deliver_sm_t *res, switch_event_t **event)
{
    char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
    switch_event_t *evt = NULL;
    char *str = NULL;

    char sql[1024] = {0};
    const char *gsm7_utf8;
    unsigned char output[255] = {0};

    int message_type = 0;

    int sar_segment_seqnum = 0;
    int sar_total_segments = 0;
    int sar_msg_ref_num    = 0;

    switch_uuid_str(uuid_str, sizeof(uuid_str));

    if (switch_event_create(&evt, SWITCH_EVENT_MESSAGE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create new event\n"); 
    }

    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "endpoint", "mod_smpp");

    str = switch_mprintf("%d", res->sequence_number);
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "sequence_number", str);

    str = switch_mprintf("%d", res->command_status);
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "command_status", str);

    str = switch_mprintf("%d", res->command_id);
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "command_id", str);

    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "smpp_gateway", gateway->name);
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "proto", "smpp");
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "msg_uuid", uuid_str);
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "sip_smpp_gw_uuid", gateway->sip_smpp_gw_uuid);

    str = switch_mprintf("%d", res->source_addr_ton);
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "source_addr_ton", str);

    str = switch_mprintf("%d", res->source_addr_npi);
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "source_addr_npi", str);

    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "from_user", (const char *) res->source_addr);

    str = switch_mprintf("%d", res->dest_addr_ton);
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "dest_addr_ton", str);

    str = switch_mprintf("%d", res->dest_addr_npi);
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "dest_addr_npi", str);

    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "to_user", (const char *) res->destination_addr);

    str = switch_mprintf("%d", res->data_coding);
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "data_coding", str);

    str = switch_mprintf("%d", res->esm_class);
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "esm_class", str);

    /* Message Mode , bitmask = .... ..XX */
    str = switch_mprintf("%d",(res->esm_class&0x03));
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "message_mode", str);

    /* Message Type , bitmask = ..XX XX.. */
    message_type = ((res->esm_class&0x3c)>>2);
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "message_type", switch_mprintf("%d",message_type));

    /* GSM Features , bitmask = XX.. .... */
    str = switch_mprintf("%d",((res->esm_class&0xc0)>>6));
    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "gsm_features", str);

    str = NULL;

    switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "profile", gateway->profile);

    switch(res->data_coding) {
	case 0:
		gsm7_utf8 = mod_smpp_gsm7_to_utf8((unsigned char *)res->short_message,res->sm_length);
		switch_event_add_body(evt, "%s", gsm7_utf8);
		break;
	case 3:
		switch_event_add_body(evt, "%s", res->short_message);
		break;
	case 4:
		break;
	case 8:
		mod_smpp_ucs2_to_utf8((unsigned char *)res->short_message,output);
		switch_event_add_body(evt, "%s", (const char *) output);
		break;
    }

    /* TLVs decoding: */
    if ( res->tlv != NULL ) {
		tlv_t *tlv = &res->tlv[0];
	
		while ( tlv != NULL ) {
			switch ( tlv->tag ) {
			case TLVID_sar_segment_seqnum:
				sar_segment_seqnum = tlv->value.octet[0]; // ?
				switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "sar_segment_seqnum", switch_mprintf("%d",sar_segment_seqnum));
				break;
			case TLVID_sar_total_segments:
				sar_total_segments = tlv->value.octet[0]; // ?
				switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "sar_total_segments", switch_mprintf("%d", sar_total_segments));
				break;
			case TLVID_sar_msg_ref_num:
				sar_msg_ref_num = tlv->value.val16; // ?
				switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "sar_msg_ref_num", switch_mprintf("%d", sar_msg_ref_num));
				break;
			case TLVID_receipted_message_id:
				str = switch_mprintf("%s", tlv->value.octet);
				switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "receipted_message_id", str);
				break;
			case TLVID_message_state:
				str = switch_mprintf("%d", tlv->value.val08);
				switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "message_state", str);
				break;
			case TLVID_network_error_code:
				str = switch_mprintf("%s", tlv->value.octet);
				switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "network_error_code", str);
			}
	
			tlv = tlv->next;
		}
    }

    if(message_type == 0) {
	sprintf(sql,"insert into message_scheduler "
		"(sms_type,orig_msisdn,dest_msisdn,transaction_id,msg_send_sm_ts,short_msg,short_msg_len,sip_smpp_gw_uuid,orig_smpp_gw,"
		"sar_total_segments,sar_segment_seqnum,sar_msg_ref_num,msg_state_id,msg_uuid) "
		"values ('sip','%s','%s',%d,%d,'%s',%d,'%s','%s',""%d,%d,%d,1,'%s')",
		res->source_addr,res->destination_addr,res->sequence_number,(int)time(NULL),
		switch_event_get_body(evt),res->sm_length,gateway->sip_smpp_gw_uuid,gateway->name,
		sar_total_segments,sar_segment_seqnum,sar_msg_ref_num,uuid_str);

	switch_mutex_lock(mod_smpp_globals.db_mutex);

	if(switch_pgsql_send_query(mod_smpp_globals.pgsql,sql) == SWITCH_PGSQL_SUCCESS) {
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "insert DELIVER_SM: %s\n",sql);
	}

	switch_pgsql_flush(mod_smpp_globals.pgsql);

	switch_mutex_unlock(mod_smpp_globals.db_mutex);
    }

    *event = evt;

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t mod_smpp_message_destroy(mod_smpp_message_t **msg)
{
    mod_smpp_message_t *tmp;

    if ( msg ) {
		tmp = *msg;
	
//		if(tmp->req.tlv != NULL) destroy_tlv(tmp->req.tlv);
	
		switch_safe_free(*msg);
    }

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t mod_smpp_message_multi_destroy(mod_smpp_message_multi_t **msg)
{
    mod_smpp_message_multi_t *tmp;

    if ( msg ) {
		tmp = *msg;
	
//		if(tmp->req.tlv != NULL) destroy_tlv(tmp->req.tlv);
	
		switch_safe_free(*msg);
    }

    return SWITCH_STATUS_SUCCESS;
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
