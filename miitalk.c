/*

    mii-tool: monitor and control the MII for a network interface

    Usage:

    mii-tool [-VvRrw] [-A media,... | -F media] [interface ...]

    This program is based on Donald Becker's "mii-diag" program, which
    is more capable and verbose than this tool, but also somewhat
    harder to use.

    Copyright (C) 2000 David A. Hinds -- dhinds@pcmcia.sourceforge.org

    mii-diag is written/copyright 1997-2000 by Donald Becker
        <becker@scyld.com>

    This program is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation.

    Donald Becker may be reached as becker@scyld.com, or C/O
    Scyld Computing Corporation, 410 Severn Av., Suite 210,
    Annapolis, MD 21403

    References
    http://www.scyld.com/diag/mii-status.html
    http://www.scyld.com/expert/NWay.html
    http://www.national.com/pf/DP/DP83840.html
*/

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <time.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#ifndef __GLIBC__
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#endif
#include "mii.h"

#define MAX_ETH     8       /* Maximum # of interfaces */


/*--------------------------------------------------------------------*/

struct option longopts[] = {
 /* { name  has_arg  *flag  val } */
    {"read",        1, 0, 'r'},
    {"write",       1, 0, 'w'},
    {"interface",   1, 0, 'i'},
    {"help",        0, 0, '?'}, /* Give help */
    { 0, 0, 0, 0 }
};


/*--------------------------------------------------------------------*/

static int mdio_read(int skfd, struct ifreq *ifr, int location)
{
    struct mii_data *mii = (struct mii_data *)&ifr->ifr_data;
    mii->reg_num = location;
    if (ioctl(skfd, SIOCGMIIREG, ifr) < 0) {
        fprintf(stderr, "SIOCGMIIREG on %s failed: %s\n", ifr->ifr_name,
            strerror(errno));
        return -1;
    }
    return mii->val_out;
}

static int mdio_write(int skfd, struct ifreq *ifr, int location, int value)
{
    struct mii_data *mii = (struct mii_data *)&ifr->ifr_data;
    mii->reg_num = location;
    mii->val_in = value;
    if (ioctl(skfd, SIOCSMIIREG, ifr) < 0) {
        fprintf(stderr, "SIOCSMIIREG on %s failed: %s\n", ifr->ifr_name,
            strerror(errno));
        return -1;
    }
    return 0;
}

/*--------------------------------------------------------------------*/

static int set_interface(int skfd, struct ifreq *ifr, char *ifname) {
    /* Get the vitals from the interface. */
    strncpy(ifr->ifr_name, ifname, IFNAMSIZ);
    if (ioctl(skfd, SIOCGMIIPHY, ifr) < 0) {
        fprintf(stderr, "SIOCGMIIPHY on '%s' failed: %s\n",
                ifname, strerror(errno));
        return 1;
    }
    return 0;
}

/*--------------------------------------------------------------------*/

const char *usage = "usage: %s [-i interface] [-w addr=val] [-r addr]\n"
"       -i, --interface             change to operate on new interface\n"
"       -w, --write                 monitor for link status changes\n"
"       -r, --read                  with -w, write events to syslog\n"
"Note: you can specify -r and -w multiple times for multiple accesses\n"
;

int main(int argc, char **argv)
{
    int c;
    int skfd = -1;       /* AF_INET socket for ioctl() calls. */
    struct ifreq ifr;
    char interface[12];
    char *val_start;
    int addr;
    int val;

    memset(&ifr, 0, sizeof(ifr));
    snprintf(interface, sizeof(interface)-1, "eth0");

    if (argc == 1) {
        printf(usage, argv[0]);
        return 0;
    }

    /* Open a basic socket. */
    if ((skfd = socket(AF_INET, SOCK_DGRAM,0)) < 0) {
        perror("socket");
        exit(-1);
    }

    set_interface(skfd, &ifr, interface);
    
    while ((c = getopt_long(argc, argv, "i:w:r:?", longopts, 0)) != EOF)
        switch (c) {
            case 'i':
                strncpy(interface, optarg, sizeof(interface));
                if (set_interface(skfd, &ifr, interface))
                    return 1;
                break;

            case 'w':
                addr = strtoul(optarg, &val_start, 0);
                if (!val_start) {
                    printf("Syntax must be 'address=value'\n");
                    break;
                }
                val = strtoul(val_start+1, NULL, 0);
                mdio_write(skfd, &ifr, addr, val);
                printf("Set register 0x%02x -> 0x%02x\n", addr, val);
                break;

            case 'r':
                addr = strtoul(optarg, NULL, 0);
                printf("Register 0x%02x <- ", addr);
                printf ("0x%02x\n", mdio_read(skfd, &ifr, addr));
                break;

            default:
            case '?':
                printf(usage, argv[0]);
                return 0;
        }

    close(skfd);
    return 0;
}
