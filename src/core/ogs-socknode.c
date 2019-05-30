/*
 * Copyright (C) 2019 by Sukchan Lee <acetcom@gmail.com>
 *
 * This file is part of Open5GS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ogs-core.h"

#undef OGS_LOG_DOMAIN
#define OGS_LOG_DOMAIN __ogs_sock_domain

ogs_socknode_t *ogs_socknode_new(
        int family, const char *hostname, uint16_t port, int flags)
{
    int rv;
    ogs_socknode_t *node = NULL;

    node = ogs_calloc(1, sizeof(ogs_socknode_t));
    rv = ogs_getaddrinfo(&node->addr, family, hostname, port, flags);
    ogs_assert(rv == OGS_OK);

    return node;
}

void ogs_socknode_free(ogs_socknode_t *node)
{
    ogs_freeaddrinfo(node->addr);
    ogs_free(node);
}

ogs_socknode_t *ogs_socknode_add(
        ogs_list_t *list, int family, ogs_sockaddr_t *sa_list)
{
    int rv;
    ogs_sockaddr_t *newaddr = NULL;
    ogs_socknode_t *node = NULL;

    ogs_assert(list);
    ogs_assert(sa_list);

    rv = ogs_copyaddrinfo(&newaddr, sa_list);
    ogs_assert(rv == OGS_OK);

    if (family != AF_UNSPEC) {
        rv = ogs_filteraddrinfo(&newaddr, family);
        ogs_assert(rv == OGS_OK);
    }

    if (newaddr) {
        node = ogs_calloc(1, sizeof(ogs_socknode_t));
        node->addr = newaddr;
        ogs_list_add(list, node);
    }

    return node;
}

void ogs_socknode_remove(ogs_list_t *list, ogs_socknode_t *node)
{
    ogs_assert(node);

    ogs_list_remove(list, node);
    ogs_socknode_free(node);
}

void ogs_socknode_remove_all(ogs_list_t *list)
{
    ogs_socknode_t *node = NULL, *saved_node = NULL;

    ogs_list_for_each_safe(list, saved_node, node)
        ogs_socknode_remove(list, node);
}

void ogs_socknode_shutdown_all(ogs_list_t *list)
{
    ogs_socknode_t *snode;

    ogs_assert(list);

    ogs_list_for_each(list, snode)
        ogs_sock_destroy(snode->sock);
}

int ogs_socknode_probe(
        ogs_list_t *list, ogs_list_t *list6, const char *dev, uint16_t port)
{
#if defined(HAVE_GETIFADDRS)
    ogs_socknode_t *node = NULL;
	struct ifaddrs *iflist, *cur;
    int rc;

	rc = getifaddrs(&iflist);
    if (rc != 0) {
        ogs_log_message(OGS_LOG_ERROR, ogs_socket_errno, "getifaddrs failed");
        return OGS_ERROR;
    }

	for (cur = iflist; cur != NULL; cur = cur->ifa_next) {
        ogs_sockaddr_t *addr = NULL;

        if (cur->ifa_flags & IFF_LOOPBACK)
            continue;

        if (cur->ifa_flags & IFF_POINTOPOINT)
            continue;

		if (cur->ifa_addr == NULL) /* may happen with ppp interfaces */
			continue;

        if (dev && strcmp(dev, cur->ifa_name) != 0)
            continue;

        addr = (ogs_sockaddr_t *)cur->ifa_addr;
        if (cur->ifa_addr->sa_family == AF_INET) {
            if (!list) continue;

#ifndef IN_IS_ADDR_LOOPBACK
#define IN_IS_ADDR_LOOPBACK(a) \
  ((((long int) (a)->s_addr) & ntohl(0xff000000)) == ntohl(0x7f000000))
#endif /* IN_IS_ADDR_LOOPBACK */

/* An IP equivalent to IN6_IS_ADDR_UNSPECIFIED */
#ifndef IN_IS_ADDR_UNSPECIFIED
#define IN_IS_ADDR_UNSPECIFIED(a) \
  (((long int) (a)->s_addr) == 0x00000000)
#endif /* IN_IS_ADDR_UNSPECIFIED */
            if (IN_IS_ADDR_UNSPECIFIED(&addr->sin.sin_addr) ||
                IN_IS_ADDR_LOOPBACK(&addr->sin.sin_addr))
                continue;
        } else if (cur->ifa_addr->sa_family == AF_INET6) {
            if (!list6) continue;

            if (IN6_IS_ADDR_UNSPECIFIED(&addr->sin6.sin6_addr) ||
                IN6_IS_ADDR_LOOPBACK(&addr->sin6.sin6_addr) ||
                IN6_IS_ADDR_MULTICAST(&addr->sin6.sin6_addr) ||
                IN6_IS_ADDR_LINKLOCAL(&addr->sin6.sin6_addr) ||
                IN6_IS_ADDR_SITELOCAL(&addr->sin6.sin6_addr))
                continue;
        } else
            continue;

        addr = ogs_calloc(1, sizeof(ogs_sockaddr_t));
        memcpy(&addr->sa, cur->ifa_addr, ogs_sockaddr_len(cur->ifa_addr));
        addr->ogs_sin_port = htons(port);

        node = ogs_calloc(1, sizeof(ogs_socknode_t));
        node->addr = addr;

        if (addr->ogs_sa_family == AF_INET) {
            ogs_assert(list);
            ogs_list_add(list, node);
        } else if (addr->ogs_sa_family == AF_INET6) {
            ogs_assert(list6);
            ogs_list_add(list6, node);
        } else
            ogs_assert_if_reached();
	}

	freeifaddrs(iflist);
    return OGS_OK;
#elif defined(_WIN32)
    return OGS_OK;
#else
    ogs_assert_if_reached();
    return OGS_ERROR;
#endif

}

int ogs_socknode_fill_scope_id_in_local(ogs_sockaddr_t *sa_list)
{
#if defined(HAVE_GETIFADDRS)
	struct ifaddrs *iflist = NULL, *cur;
    int rc;
    ogs_sockaddr_t *addr, *ifaddr;

    for (addr = sa_list; addr != NULL; addr = addr->next) {
        if (addr->ogs_sa_family != AF_INET6)
            continue;

        if (!IN6_IS_ADDR_LINKLOCAL(&addr->sin6.sin6_addr))
            continue;

        if (addr->sin6.sin6_scope_id != 0)
            continue;

        if (iflist == NULL) {
            rc = getifaddrs(&iflist);
            if (rc != 0) {
                ogs_log_message(OGS_LOG_ERROR, ogs_socket_errno,
                        "getifaddrs failed");
                return OGS_ERROR;
            }
        }

        for (cur = iflist; cur != NULL; cur = cur->ifa_next) {
            ifaddr = (ogs_sockaddr_t *)cur->ifa_addr;

            if (cur->ifa_addr == NULL) /* may happen with ppp interfaces */
                continue;

            if (cur->ifa_addr->sa_family != AF_INET6)
                continue;

            if (!IN6_IS_ADDR_LINKLOCAL(&ifaddr->sin6.sin6_addr))
                continue;

            if (memcmp(&addr->sin6.sin6_addr,
                    &ifaddr->sin6.sin6_addr, sizeof(struct in6_addr)) == 0) {
                /* Fill Scope ID in localhost */
                addr->sin6.sin6_scope_id = ifaddr->sin6.sin6_scope_id;
            }
        }
    }

    if (iflist)
        freeifaddrs(iflist);

    return OGS_OK;
#elif defined(_WIN32)
    return OGS_OK;
#else
    ogs_assert_if_reached();
    return OGS_ERROR;
#endif
}
