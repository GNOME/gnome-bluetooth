/*********************************************************************
 *
 * Filename:      obex_test_server.c
 * Version:       0.8
 * Description:   Server OBEX Commands
 * Status:        Experimental.
 * Author:        Pontus Fuchs <pontus.fuchs@tactel.se>
 * Created at:    Sun Aug 13 03:00:28 PM CEST 2000
 * Modified at:   Sun Aug 13 03:00:33 PM CEST 2000
 * Modified by:   Pontus Fuchs <pontus.fuchs@tactel.se>
 *
 *     Copyright (c) 2000, Pontus Fuchs, All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <openobex/obex.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "obex_io.h"
#include "obex_test.h"
#include "obex_test_server.h"

#define TRUE  1
#define FALSE 0

void put_server(obex_t *handle, obex_object_t *object);
void connect_server(obex_t *handle, obex_object_t *object);

void
put_server(obex_t *handle, obex_object_t *object)
{
	obex_headerdata_t hv;
	uint8_t hi;
	int hlen;

	const uint8_t *body = NULL;
	int body_len = 0;
	char *name = NULL;
	char *namebuf = NULL;

	struct context *gt;
	gt = OBEX_GetUserData(handle);

	printf(/* __FUNCTION */ "()\n");

	while(OBEX_ObjectGetNextHeader(handle, object, &hi, &hv, &hlen))	{
		switch(hi)	{
		case OBEX_HDR_BODY:
			printf(/* __FUNCTION */ "() Found body\n");
			body = hv.bs;
			body_len = hlen;
			break;
		case OBEX_HDR_NAME:
			printf(/* __FUNCTION */ "() Found name\n");
			if( (namebuf = malloc(hlen / 2)))	{
				OBEX_UnicodeToChar(namebuf, hv.bs, hlen);
				name = namebuf;
			}
			break;

		default:
			printf(/* __FUNCTION */ "() Skipped header %02x\n", hi);
		}
	}
	if(!body)	{
		printf("Got a PUT without a body\n");
		return;
	}
	if(!name)	{
		name = "OBEX_PUT_Unknown_object";
		printf("Got a PUT without a name. Setting name to %s\n", name);

	}
	safe_save_file(name, body, body_len, gt->save_dir);
	free(namebuf);
}

void
connect_server(obex_t *handle, obex_object_t *object)
{
	obex_headerdata_t hv;
	uint8_t hi;
	int hlen;

	const uint8_t *who = NULL;
	int who_len = 0;
	printf(/* __FUNCTION */ "()\n");

	while(OBEX_ObjectGetNextHeader(handle, object, &hi, &hv, &hlen))	{
		if(hi == OBEX_HDR_WHO)	{
			who = hv.bs;
			who_len = hlen;
		}
		else	{
			printf(/* __FUNCTION */ "() Skipped header %02x\n", hi);
		}
	}
	if (who_len == 6)	{
		if(strncmp("Linux", who, 6) == 0)	{
			printf("Weeeha. I'm talking to the coolest OS ever!\n");
		}
	}
	OBEX_ObjectSetRsp(object, OBEX_RSP_SUCCESS, OBEX_RSP_SUCCESS);
}

void
server_request(obex_t *handle, obex_object_t *object, int event, int cmd)
{
	switch(cmd)	{
	case OBEX_CMD_CONNECT:
		connect_server(handle, object);
		break;
	case OBEX_CMD_DISCONNECT:
		printf("We got a disconnect-request\n");
		OBEX_ObjectSetRsp(object, OBEX_RSP_SUCCESS, OBEX_RSP_SUCCESS);
		break;
	case OBEX_CMD_PUT:
		OBEX_ObjectSetRsp(object, OBEX_RSP_CONTINUE, OBEX_RSP_SUCCESS);
		put_server(handle, object);
		break;
	case OBEX_CMD_SETPATH:
		printf("Got a SETPATH request\n");
		OBEX_ObjectSetRsp(object, OBEX_RSP_CONTINUE, OBEX_RSP_SUCCESS);
		break;
	case OBEX_CMD_GET:
	default:
		printf(/* __FUNCTION */ "() Denied %02x request\n", cmd);
		OBEX_ObjectSetRsp(object, OBEX_RSP_NOT_IMPLEMENTED, OBEX_RSP_NOT_IMPLEMENTED);
		break;
	}
	return;
}

void
server_done(obex_t *handle, obex_object_t *object, int obex_cmd, int obex_rsp)
{
	struct context *gt;
	gt = OBEX_GetUserData(handle);

	printf("Server request finished!\n");

	switch (obex_cmd) {
	case OBEX_CMD_DISCONNECT:
		printf("Disconnect done!\n");
		OBEX_TransportDisconnect(handle);
		gt->serverdone = TRUE;
		break;

	default:
		printf(/* __FUNCTION */ "() Command (%02x) has now finished\n", 
			   obex_cmd);
		break;
	}
}
