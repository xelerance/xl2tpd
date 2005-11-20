/*
 * Layer Two Tunnelling Protocol Daemon
 * Copyright (C) 1998 Adtran, Inc.
 * Copyright (C) 2002 Jeff McAdams
 *
 * Mark Spencer
 *
 * This software is distributed under the terms
 * of the GPL, which you should have received
 * along with this source.
 *
 * Attribute Value Pair creating routines
 */

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "l2tp.h"

/* 
 * These routines should add avp's to a buffer
 * to be sent
 */


/* FIXME:  If SANITY is on, we should check for buffer overruns */

/* FIXME: Can't this be condensed alot? */

int add_message_type_avp (struct buffer *buf, _u16 type)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0x8 | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = 0;
    raw[3] = htons (type);
    buf->len += 8;
    return 0;
}

int add_protocol_avp (struct buffer *buf)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0x8 | MBIT);        /* Length and M bit */
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x2);       /* Value of our AVP */
    raw[3] = htons (OUR_L2TP_VERSION);
    buf->len += 8;
    return 0;
}

int add_frame_caps_avp (struct buffer *buf, _u16 caps)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0xA | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x3);
    raw[3] = 0;
    raw[4] = htons (caps);
    buf->len += 10;
    return 0;
}

int add_bearer_caps_avp (struct buffer *buf, _u16 caps)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0xA | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x4);
    raw[3] = 0;
    raw[4] = htons (caps);
    buf->len += 10;
    return 0;
}

/* FIXME: I need to send tie breaker AVP's */

int add_firmware_avp (struct buffer *buf)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0x8);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x6);
    raw[3] = htons (FIRMWARE_REV);
    buf->len += 8;
    return 0;
}

/*
int add_hostname_avp(struct buffer *buf) {
	_u16 *raw = (_u16 *)(buf->start + buf->len);
	raw[0] = htons((0x6 + strlen(hostname)) | MBIT);
	raw[1] = htons(VENDOR_ID);
	raw[2] = htons(0x7);
	strcpy((char *)(&raw[3]), hostname);
	buf->len += 6 + strlen(hostname);
	return 0;
}
*/

int add_hostname_avp (struct buffer *buf)
{
    char names[6] = "eriwan";
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0xC | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x7);
    strcpy ((char *) (&raw[3]), names);
    buf->len += 12;
    return 0;
}

int add_vendor_avp (struct buffer *buf)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0x6 + strlen (VENDOR_NAME));
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x8);
    strcpy ((char *) (&raw[3]), VENDOR_NAME);
    buf->len += 6 + strlen (VENDOR_NAME);
    return 0;
}

int add_tunnelid_avp (struct buffer *buf, _u16 tid)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0x8 | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x9);
    raw[3] = htons (tid);
    buf->len += 8;
    return 0;
}

int add_avp_rws (struct buffer *buf, _u16 rws)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0x8 | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0xA);
    raw[3] = htons (rws);
    buf->len += 8;
    return 0;
}

int add_challenge_avp (struct buffer *buf, char *c, int len)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons ((0x6 + len) | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0xB);
    bcopy (c, (char *) (&raw[3]), len);
    buf->len += 6 + len;
    return 0;
}

int add_chalresp_avp (struct buffer *buf, char *c, int len)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons ((0x6 + len) | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0xD);
    bcopy (c, (char *) (&raw[3]), len);
    buf->len += 6 + len;
    return 0;
}

int add_randvect_avp (struct buffer *buf, char *c, int len)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons ((0x6 + len) | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x24);
    bcopy (c, (char *) (&raw[3]), len);
    buf->len += 6 + len;
    return 0;
}

int add_result_code_avp (struct buffer *buf, _u16 result, _u16 error,
                         char *msg, int len)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons ((0xA + len) | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x1);
    raw[3] = htons (result);
    raw[4] = htons (error);
    bcopy (msg, (char *) &raw[5], len);
    buf->len += (10 + len);
    return 0;
}

#ifdef TEST_HIDDEN
int add_callid_avp (struct buffer *buf, _u16 callid, struct tunnel *t)
{
#else
int add_callid_avp (struct buffer *buf, _u16 callid)
{
#endif
    _u16 *raw = (_u16 *) (buf->start + buf->len);
#ifdef TEST_HIDDEN
    if (t->hbit)
        raw++;
#endif
    raw[0] = htons (0x8 | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0xE);
    raw[3] = htons (callid);
    buf->len += 8;
#ifdef TEST_HIDDEN
    if (t->hbit)
        encrypt_avp (buf, 8, t);
#endif
    return 0;
}

int add_serno_avp (struct buffer *buf, unsigned int serno)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0xA | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0xF);
    raw[3] = htons ((serno >> 16) & 0xFFFF);
    raw[4] = htons (serno & 0xFFFF);
    buf->len += 10;
    return 0;
}

int add_bearer_avp (struct buffer *buf, int bearer)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0xA | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x12);
    raw[3] = htons ((bearer >> 16) & 0xFFFF);
    raw[4] = htons (bearer & 0xFFFF);
    buf->len += 10;
    return 0;
}

int add_frame_avp (struct buffer *buf, int frame)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0xA | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x13);
    raw[3] = htons ((frame >> 16) & 0xFFFF);
    raw[4] = htons (frame & 0xFFFF);
    buf->len += 10;
    return 0;
}

int add_txspeed_avp (struct buffer *buf, int speed)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0xA | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x18);
    raw[3] = htons ((speed >> 16) & 0xFFFF);
    raw[4] = htons (speed & 0xFFFF);
    buf->len += 10;
    return 0;
}

int add_rxspeed_avp (struct buffer *buf, int speed)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0xA | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x26);
    raw[3] = htons ((speed >> 16) & 0xFFFF);
    raw[4] = htons (speed & 0xFFFF);
    buf->len += 10;
    return 0;
}

int add_physchan_avp (struct buffer *buf, unsigned int physchan)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0x8 | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x19);
    raw[3] = htons ((physchan >> 16) & 0xFFFF);
    raw[4] = htons (physchan & 0xFFFF);
    buf->len += 10;
    return 0;
}

int add_ppd_avp (struct buffer *buf, _u16 ppd)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0x8 | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x14);
    raw[3] = htons (ppd);
    buf->len += 8;
    return 0;
}

int add_seqreqd_avp (struct buffer *buf)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0x6 | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x27);
    buf->len += 6;
    return 0;
}

/* jz: options dor the outgoing call */

/* jz: Minimum BPS - 16 */
int add_minbps_avp (struct buffer *buf, int speed)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0xA | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x10);
    raw[3] = htons ((speed >> 16) & 0xFFFF);
    raw[4] = htons (speed & 0xFFFF);
    buf->len += 10;
    return 0;
}

/* jz: Maximum BPS - 17 */
int add_maxbps_avp (struct buffer *buf, int speed)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons (0xA | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x11);
    raw[3] = htons ((speed >> 16) & 0xFFFF);
    raw[4] = htons (speed & 0xFFFF);
    buf->len += 10;
    return 0;
}

/* jz: Dialed Number 21 */
int add_number_avp (struct buffer *buf, char *no)
{
    _u16 *raw = (_u16 *) (buf->start + buf->len);
    raw[0] = htons ((0x6 + strlen (no)) | MBIT);
    raw[1] = htons (VENDOR_ID);
    raw[2] = htons (0x15);
    strncpy ((char *) (&(raw[3])), no, strlen (no));
    buf->len += 6 + strlen (no);
    return 0;
}
