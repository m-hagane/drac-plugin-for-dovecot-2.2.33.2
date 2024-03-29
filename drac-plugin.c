/*
 *
 * dovecot plugin for DRAC authentication
 *
 * Copyright (C) 2011 DesigNET, INC.
 * Updated 2024/03/12 by Masanobu Hagane
 *
 * $Id: drac-plugin.c,v 1.1.1.1 2011/09/09 08:04:34 usuda Exp $
 *
 * based:
 *   http://dovecot.org/patches/1.1/drac.c
 */
#include "lib.h"
#include "net.h"
#include "ioloop.h"
#include "mail-user.h"
#include "mail-storage-private.h"
#include <stdlib.h>
#include <string.h>

/* default parameters */
#define DRAC_TIMEOUT_SECS	(60)
#define DRAC_HOST		"localhost"


/* libdrac function */
int dracauth(char *, unsigned long, char **);

const char *drac_plugin_version = DOVECOT_ABI_VERSION;

static struct timeout *to_drac = NULL;
static const char *drachost = NULL; /* dracd host */
static unsigned long in_ip = 0; /* remote ip address */
static unsigned long dractout = 0; /* drac timeout secs */

static void drac_timeout(void *context ATTR_UNUSED)
{
    char *err;

    if(dracauth((char *)drachost, in_ip, &err) != 0) {
        i_error("%s: dracauth() failed: %s", __FUNCTION__, err);
    }
}

static void drac_mail_user_created(struct mail_user *user)
{
    const char *dractout_str;
    char addrname[256];
    char *ep;

    /* check address family */
    if(user->conn.remote_ip->family != AF_INET) {
        if(inet_ntop(user->conn.remote_ip->family, &user->conn.remote_ip->u,
           addrname, sizeof(addrname)-1) == NULL) {
            strcpy(addrname, "<unknown>");
        }
        i_error("%s: Only IPv4 addresses are supported (%s)", __FUNCTION__,
                addrname);
    } else {
        /* get remote IPv4 address... uum... */
        memcpy(&in_ip, &user->conn.remote_ip->u, sizeof(in_ip));

        /* get DRAC server name */
        drachost = mail_user_plugin_getenv(user, "dracdserver");
        if(drachost == NULL) {
            drachost = DRAC_HOST;
        }

        /* get timeout secs */
        dractout_str = mail_user_plugin_getenv(user, "dracdtimeout");
        if(dractout_str == NULL) {
            dractout = DRAC_TIMEOUT_SECS;
        } else {
            dractout = strtoul(dractout_str, &ep, 10);
            /* bad format -> use default value */
            if(ep != NULL && *ep != '\0') {
                i_warning("%s: bad dracdtimeout (%s). using default %d",
                          __FUNCTION__, dractout_str, DRAC_TIMEOUT_SECS);
                dractout = DRAC_TIMEOUT_SECS;
            }
        }
        i_debug("%s: dracdserver=%s, timeout=%ldsecs", __FUNCTION__,
               drachost, dractout);

        /* connect to DRAC server */
        drac_timeout(NULL);
        to_drac = timeout_add(1000*dractout, drac_timeout, NULL);
    }
}

static struct mail_storage_hooks drac_mail_storage_hooks = {
    .mail_user_created = drac_mail_user_created,
};

void drac_plugin_init(struct module *module)
{
    mail_storage_hooks_add(module, &drac_mail_storage_hooks);
    i_debug("%s called", __FUNCTION__);
}

void drac_plugin_deinit(void)
{
    if(to_drac != NULL) {
        timeout_remove(&to_drac);
        to_drac = NULL;
    }
    mail_storage_hooks_remove(&drac_mail_storage_hooks);
    i_debug("%s called", __FUNCTION__);
}
