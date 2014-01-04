/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/***
  This file is part of the Discontent Management System.
 
  Copyright 2014 David Strauss <david@davidstrauss.net>
  Copyright 2012 Lennart Poettering
  Copyright 2012 Zbigniew Jędrzejewski-Szmek
 
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
***/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <systemd/sd-journal.h>
#include <systemd/sd-daemon.h>
#include <mkdio.h>
#include <microhttpd.h>

#define _SD_XSTRINGIFY(x) #x
#define _SD_STRINGIFY(x) _SD_XSTRINGIFY(x)
#define _cleanup_(x) __attribute__((cleanup(x)))
#define _cleanup_free_ _cleanup_(freep)
#define log_oom() log_oom_internal("CODE_FILE=" __FILE__, "CODE_LINE=" _SD_STRINGIFY(__LINE__), __func__)
#define respond_oom(connection) log_oom(), respond_oom_internal(connection)

static inline void freep(void *p) {
        free(*(void**) p);
}

int log_oom_internal(const char *file, const char *line, const char *func) {
        sd_journal_print_with_location(LOG_ERR, file, line, func, "Out of memory.");
        return -ENOMEM;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
void microhttpd_logger(void *arg, const char *fmt, va_list ap) {
        _cleanup_free_ char *f;
        if (asprintf(&f, "microhttpd: %s", fmt) <= 0) {
                log_oom();
                return;
        }
        sd_journal_printv(LOG_INFO, f, ap);
}
#pragma GCC diagnostic pop

static int respond_oom_internal(struct MHD_Connection *connection) {
        struct MHD_Response *response;
        const char m[] = "Out of memory.\n";
        int ret;

        assert(connection);

        response = MHD_create_response_from_buffer(sizeof(m)-1, (char*) m, MHD_RESPMEM_PERSISTENT);
        if (!response)
                return MHD_NO;

        MHD_add_response_header(response, "Content-Type", "text/plain");
        ret = MHD_queue_response(connection, MHD_HTTP_SERVICE_UNAVAILABLE, response);
        MHD_destroy_response(response);

        return ret;
}

static int respond_error(
                struct MHD_Connection *connection,
                unsigned code,
                const char *format, ...) {

        struct MHD_Response *response;
        char *m;
        int r;
        va_list ap;

        assert(connection);
        assert(format);

        va_start(ap, format);
        r = vasprintf(&m, format, ap);
        va_end(ap);

        if (r < 0)
                return respond_oom(connection);

        response = MHD_create_response_from_buffer(strlen(m), m, MHD_RESPMEM_MUST_FREE);
        if (!response) {
                free(m);
                return respond_oom(connection);
        }

        MHD_add_response_header(response, "Content-Type", "text/plain");
        r = MHD_queue_response(connection, code, response);
        MHD_destroy_response(response);

        return r;
}

static int request_handler(
                void *cls,
                struct MHD_Connection *connection,
                const char *url,
                const char *method,
                const char *version,
                const char *upload_data,
                size_t *upload_data_size,
                void **connection_cls) {

        assert(connection);
        assert(connection_cls);
        assert(url);
        assert(method);

        if (strcmp(method, "GET") != 0)
                return respond_error(connection, MHD_HTTP_METHOD_NOT_ACCEPTABLE,
                                     "Unsupported method.\n");

        return respond_error(connection, MHD_HTTP_NOT_FOUND, "Not found.\n");
}

int main(int argc, char *argv[]) {
        struct MHD_Daemon *d = NULL;
        int r, n;

        n = sd_listen_fds(1);
        if (n < 0) {
                sd_journal_print(LOG_ERR, "Failed to determine passed sockets: %s", strerror(-n));
                goto finish;
        } else if (n > 1) {
                sd_journal_print(LOG_ERR, "Can't listen on more than one socket.");
                goto finish;
        } else {
                struct MHD_OptionItem opts[] = {
                        { MHD_OPTION_EXTERNAL_LOGGER,
                          (intptr_t) microhttpd_logger, NULL },
                        { MHD_OPTION_LISTEN_SOCKET,
                          SD_LISTEN_FDS_START, NULL },
                        { MHD_OPTION_END, 0, NULL }};
                int flags = MHD_USE_THREAD_PER_CONNECTION|MHD_USE_POLL|MHD_USE_DEBUG;

                d = MHD_start_daemon(flags, 19531,
                                     NULL, NULL,
                                     request_handler, NULL,
                                     MHD_OPTION_ARRAY, opts,
                                     MHD_OPTION_END);
        }

        if (!d) {
                sd_journal_print(LOG_ERR, "Failed to start daemon!");
                goto finish;
        }

        r = EXIT_SUCCESS;

finish:
        if (d)
                MHD_stop_daemon(d);

        return r;
}
