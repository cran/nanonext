// Copyright (C) 2022-2023 Hibiki AI Limited <info@hibiki-ai.com>
//
// This file is part of nanonext.
//
// nanonext is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// nanonext is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// nanonext. If not, see <https://www.gnu.org/licenses/>.

// nanonext - C level - Threaded Applications ----------------------------------

#define NANONEXT_INTERNALS
#define NANONEXT_PROTOCOLS
#define NANONEXT_SUPPLEMENTALS
#define NANONEXT_TIME
#include "nanonext.h"

// messenger -------------------------------------------------------------------

static void thread_finalizer(SEXP xptr) {

  if (R_ExternalPtrAddr(xptr) == NULL)
    return;
  nng_thread *xp = (nng_thread *) R_ExternalPtrAddr(xptr);
  nng_thread_destroy(xp);

}

static void rnng_thread(void *arg) {

  SEXP list = (SEXP) arg;
  SEXP socket = VECTOR_ELT(list, 0);
  SEXP key = VECTOR_ELT(list, 1);
  SEXP two = VECTOR_ELT(list, 2);
  nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(socket);
  unsigned char *buf;
  size_t sz;
  time_t now;
  struct tm *tms;
  int xc;

  while (1) {
    xc = nng_recv(*sock, &buf, &sz, NNG_FLAG_ALLOC);
    time(&now);
    tms = localtime(&now);

    if (xc) {
      REprintf("| messenger session ended: %d-%02d-%02d %02d:%02d:%02d\n",
               tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday,
               tms->tm_hour, tms->tm_min, tms->tm_sec);
      break;
    }

    if (!strncmp((char *) buf, ":", 1)) {
      if (!strcmp((char *) buf, ":c ")) {
        REprintf("| <- peer connected: %d-%02d-%02d %02d:%02d:%02d\n",
                 tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday,
                 tms->tm_hour, tms->tm_min, tms->tm_sec);
        nng_free(buf, sz);
        rnng_send(socket, key, two, Rf_ScalarLogical(0));
        continue;
      }
      if (!strcmp((char *) buf, ":d ")) {
        REprintf("| -> peer disconnected: %d-%02d-%02d %02d:%02d:%02d\n",
                 tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday,
                 tms->tm_hour, tms->tm_min, tms->tm_sec);
        nng_free(buf, sz);
        continue;
      }
    }

    Rprintf("%s\n%*s< %d-%02d-%02d %02d:%02d:%02d\n",
            (char *) buf, (int) sz, "",
            tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday,
            tms->tm_hour, tms->tm_min, tms->tm_sec);
    nng_free(buf, sz);

  }

}

SEXP rnng_messenger(SEXP url) {

  const char *up = CHAR(STRING_ELT(url, 0));
  nng_socket *sock = R_Calloc(1, nng_socket);
  void *dlp;
  uint8_t dialer = 0;
  int xc;
  SEXP socket, con;

  xc = nng_pair0_open(sock);
  if (xc) {
    R_Free(sock);
    ERROR_OUT(xc);
  }
  dlp = R_Calloc(1, nng_listener);
  xc = nng_listen(*sock, up, dlp, 0);
  if (xc == 10 || xc == 15) {
    R_Free(dlp);
    dlp = R_Calloc(1, nng_dialer);
    xc = nng_dial(*sock, up, dlp, 0);
    if (xc) {
      R_Free(dlp);
      R_Free(sock);
      ERROR_OUT(xc);
    }
    dialer = 1;

  } else if (xc) {
    R_Free(dlp);
    R_Free(sock);
    ERROR_OUT(xc);
  }

  PROTECT(socket = R_MakeExternalPtr(sock, nano_SocketSymbol, R_NilValue));
  R_RegisterCFinalizerEx(socket, socket_finalizer, TRUE);

  PROTECT(con = R_MakeExternalPtr(dlp, R_NilValue, R_NilValue));
  if (dialer) {
    R_RegisterCFinalizerEx(con, dialer_finalizer, TRUE);
    Rf_setAttrib(socket, nano_DialerSymbol, R_MissingArg);
  } else {
    R_RegisterCFinalizerEx(con, listener_finalizer, TRUE);
  }
  R_MakeWeakRef(socket, con, R_NilValue, TRUE);

  UNPROTECT(2);
  return socket;

}

SEXP rnng_thread_create(SEXP list) {

  SEXP socket = VECTOR_ELT(list, 0);
  nng_thread *thr;
  SEXP xptr;

  nng_thread_create(&thr, rnng_thread, list);

  PROTECT(xptr = R_MakeExternalPtr(thr, R_NilValue, R_NilValue));
  R_RegisterCFinalizerEx(xptr, thread_finalizer, TRUE);
  R_MakeWeakRef(socket, xptr, R_NilValue, TRUE);

  UNPROTECT(1);
  return socket;

}

