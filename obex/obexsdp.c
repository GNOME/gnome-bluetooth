/*

 Mostly borrowed from BlueZ

 Copyright (C) 2002 Maxim Krasnyansky <maxk@qualcomm.com>,
 Stephen Crane <steve.crane@rococosoft.com>
*/

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>

#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include "obexsdp.h"

/*
 * Support for Service (de)registration
 */
typedef struct {
    char *name;
    char *provider;
    char *desc;

    unsigned int class;
    unsigned int profile;
    unsigned int channel;
} svc_info_t;

static int
add_opush(sdp_session_t *session,
		  sdp_record_t *rec,
		  svc_info_t *si,
		  uint32_t *handle)
{
    sdp_list_t *svclass_id, *pfseq, *apseq, *root;
    uuid_t root_uuid, opush_uuid, l2cap_uuid, rfcomm_uuid, obex_uuid;
    sdp_profile_desc_t profile[1];
    sdp_list_t *aproto, *proto[3];
    sdp_record_t record;
    uint8_t chan = si->channel? si->channel: 4;
    sdp_data_t *channel;
    uint8_t formats[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
    //uint8_t formats[] = { 0xff };
    void *dtds[sizeof(formats)], *values[sizeof(formats)];
    unsigned int i;
    uint8_t dtd = SDP_UINT8;
    sdp_data_t *sflist;

    memset((void *)&record, 0, sizeof(sdp_record_t));
    record.handle = 0xffffffff;
    sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
    root = sdp_list_append(0, &root_uuid);
    sdp_set_browse_groups(&record, root);

    sdp_uuid16_create(&opush_uuid, OBEX_OBJPUSH_SVCLASS_ID);
    svclass_id = sdp_list_append(0, &opush_uuid);
    sdp_set_service_classes(&record, svclass_id);

    sdp_uuid16_create(&profile[0].uuid, OBEX_OBJPUSH_PROFILE_ID);
    profile[0].version = 0x0100;
    pfseq = sdp_list_append(0, profile);
    sdp_set_profile_descs(&record, pfseq);

    sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
    proto[0] = sdp_list_append(0, &l2cap_uuid);
    apseq = sdp_list_append(0, proto[0]);

    sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
    proto[1] = sdp_list_append(0, &rfcomm_uuid);
    channel = sdp_data_alloc(SDP_UINT8, &chan);
    proto[1] = sdp_list_append(proto[1], channel);
    apseq = sdp_list_append(apseq, proto[1]);

    sdp_uuid16_create(&obex_uuid, OBEX_UUID);
    proto[2] = sdp_list_append(0, &obex_uuid);
    apseq = sdp_list_append(apseq, proto[2]);

    aproto = sdp_list_append(0, apseq);
    sdp_set_access_protos(&record, aproto);
    sdp_data_free(channel);
    sdp_list_free(proto[0], 0);
    sdp_list_free(proto[1], 0);
    sdp_list_free(proto[2], 0);
    sdp_list_free(apseq, 0);
    sdp_list_free(aproto, 0);

    for (i = 0; i < sizeof(formats); i++) {
	  dtds[i] = &dtd;
	  values[i] = &formats[i];
    }
    sflist = sdp_seq_alloc(dtds, values, sizeof(formats));
    sdp_attr_add(&record, SDP_ATTR_SUPPORTED_FORMATS_LIST, sflist);

    sdp_set_info_attr(&record, "OBEX Object Push", 0, 0);

    if (0 > sdp_record_register(session, &record, SDP_RECORD_PERSIST)) {
	  printf("Service Record registration failed.\n");
	  return -1;
    }
	*handle=record.handle;
    printf("OBEX Object Push service registered\n");
    return 0;
}

int
register_sdp(uint32_t *handle)
{
  svc_info_t si;
  int ret;
  bdaddr_t interface;
  sdp_session_t *sess;

  bacpy(&interface, BDADDR_ANY);
  sess=sdp_connect(&interface, BDADDR_LOCAL, 0);

  if (!sess)
	return -1;
  memset(&si, 0, sizeof(si));
  si.name="OPUSH";
  ret=add_opush(sess, 0, &si, handle);
  sdp_close(sess);
  return ret;
}

int
deregister_sdp(uint32_t handle)
{
  uint32_t range = 0x0000ffff;
  sdp_list_t *attr;
  sdp_session_t *sess;
  sdp_record_t *rec;
  bdaddr_t interface;

  bacpy(&interface, BDADDR_ANY);

  sess = sdp_connect(&interface, BDADDR_LOCAL, 0);
  if (!sess) {
	printf("No local SDP server!\n");
	return -1;
  }

  attr = sdp_list_append(0, &range);
  rec = sdp_service_attr_req(sess, handle, SDP_ATTR_REQ_RANGE, attr);
  sdp_list_free(attr, 0);
  if (!rec) {
	printf("Service Record not found.\n");
	sdp_close(sess);
	return -1;
  }
  if (sdp_record_unregister(sess, rec)) {
	printf("Failed to unregister service record: %s\n", strerror(errno));
	sdp_close(sess);
	return -1;
  }
  printf("Service Record deleted.\n");
  sdp_close(sess);
  return 0;
}
