#include "bitfiend.h"
#include "list.h"
#include "peer_id.h"
#include "peer_listener.h"
#include "bencode.h"
#include "torrent.h"
#include "torrent_file.h"
#include "log.h" 

#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static pthread_t         s_peer_listener;
static const uint16_t    s_port = 6889; 
static pthread_mutex_t   s_torrents_lock = PTHREAD_MUTEX_INITIALIZER;
static list_t           *s_torrents; 

int bitfiend_init(void)
{
    log_set_lvl(LOG_LEVEL_DEBUG);    
    log_set_logfile(stdout);

    peer_id_create(g_local_peer_id);

    s_torrents = list_init();

    if(peer_listener_create(&s_peer_listener, &s_port))
        goto fail_start_listener;
    
    log_printf(LOG_LEVEL_INFO, "BitFiend init successful\n");
    return BITFIEND_SUCCESS;

fail_start_listener:
    log_printf(LOG_LEVEL_ERROR, "BitFiend init error\n");
    return BITFIEND_FAILURE;
}

static int shutdown_torrent(torrent_t *torrent)
{
    if(pthread_cancel(torrent->tracker_thread))
        goto fail_stop_tracker;

    void *ret;
    pthread_join(torrent->tracker_thread, &ret);
    assert(ret == PTHREAD_CANCELED);

    torrent_free(torrent);
    return BITFIEND_SUCCESS;

fail_stop_tracker:
    return BITFIEND_FAILURE;
}

int bitfiend_shutdown(void)
{
    int ret = BITFIEND_SUCCESS;

    if(pthread_cancel(s_peer_listener))
        ret = BITFIEND_FAILURE;

    void *tret;
    pthread_join(s_peer_listener, &tret);
    assert(tret == PTHREAD_CANCELED);

    pthread_mutex_lock(&s_torrents_lock);
    const unsigned char *entry;
    FOREACH_ENTRY(entry, s_torrents) {
        shutdown_torrent(*(torrent_t**)entry);
    }
    list_free(s_torrents);
    pthread_mutex_unlock(&s_torrents_lock);

    if(ret == BITFIEND_SUCCESS)
        log_printf(LOG_LEVEL_INFO, "BitFiend shutdown successful\n");
    else
        log_printf(LOG_LEVEL_INFO, "BitFiend shutdown error\n");

    return ret;
}

torrent_t *bitfiend_add_torrent(const char *metafile)
{
    bencode_obj_t *obj = torrent_file_parse(metafile);
    if(!obj)
        goto fail_parse;

    torrent_t *torrent = torrent_init(obj);
    extern void print_torrent(torrent_t *torrent);
    print_torrent(torrent);

    bencode_free_obj_and_data_recursive(obj);
    if(!torrent)
        goto fail_create;

    tracker_arg_t *arg = malloc(sizeof(tracker_arg_t));
    arg->torrent = torrent;
    arg->port = s_port;
    if(tracker_connection_create(&torrent->tracker_thread, arg))
        goto fail_create;

    pthread_mutex_lock(&s_torrents_lock);
    list_add(s_torrents, (unsigned char*)&torrent, sizeof(torrent_t*));
    pthread_mutex_unlock(&s_torrents_lock);

    log_printf(LOG_LEVEL_INFO, "Torrent added successfully: %s\n", metafile);
    return torrent;
    
fail_create:
fail_parse:
    log_printf(LOG_LEVEL_ERROR, "Error adding torrent: %s\n", metafile);
    return NULL;
}

int bitfiend_start_torrent(torrent_t *torrent)
{

}

int bitfiend_pause_torrent(torrent_t *torrent)
{

}

int bitfiend_set_priority(torrent_t *torrent)
{

}

int bitfiend_remove_torrent(torrent_t *torrent)
{

}

