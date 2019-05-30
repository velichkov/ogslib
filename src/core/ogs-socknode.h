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

#ifndef OGS_SOCKNODE_H
#define OGS_SOCKNODE_H

#if !defined(OGS_CORE_INSIDE)
#error "Only <ogs-core.h> can be included directly."
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ogs_poll_s ogs_poll_t;
typedef struct ogs_socknode_s {
    ogs_lnode_t node;

    ogs_sock_t *sock;
    ogs_sockaddr_t *addr;
    ogs_poll_t *poll;
} ogs_socknode_t;

ogs_socknode_t *ogs_socknode_new(
        int family, const char *hostname, uint16_t port, int flags);
void ogs_socknode_free(ogs_socknode_t *node);

ogs_socknode_t *ogs_socknode_add(
        ogs_list_t *list, int family, ogs_sockaddr_t *sa_list);
void ogs_socknode_remove(ogs_list_t *list, ogs_socknode_t *node);
void ogs_socknode_remove_all(ogs_list_t *list);

void ogs_socknode_shutdown_all(ogs_list_t *list);

int ogs_socknode_probe(
        ogs_list_t *list, ogs_list_t *list6, const char *dev, uint16_t port);
int ogs_socknode_fill_scope_id_in_local(ogs_sockaddr_t *sa_list);

#ifdef __cplusplus
}
#endif

#endif /* OGS_SOCKNODE_H */
