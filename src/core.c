/* nanonext - C level - Core Functions -------------------------------------- */

#include <nng/nng.h>
#include "nanonext.h"


/* utils -------------------------------------------------------------------- */

SEXP rnng_strerror(SEXP error) {

  int xc = INTEGER(error)[0];
  const char *err = nng_strerror(xc);
  return Rf_mkString(err);

}

SEXP rnng_version(void) {

  const char *ver = nng_version();
  return Rf_mkString(ver);

}

/* external pointer finalisers ---------------------------------------------- */

static void context_finalizer(SEXP xptr) {

  if (R_ExternalPtrAddr(xptr) == NULL)
    return;
  nng_ctx *xp = (nng_ctx *) R_ExternalPtrAddr(xptr);
  R_Free(xp);
  R_ClearExternalPtr(xptr);

}

static void dialer_finalizer(SEXP xptr) {

  if (R_ExternalPtrAddr(xptr) == NULL)
    return;
  nng_dialer *xp = (nng_dialer *) R_ExternalPtrAddr(xptr);
  R_Free(xp);
  R_ClearExternalPtr(xptr);

}

static void listener_finalizer(SEXP xptr) {

  if (R_ExternalPtrAddr(xptr) == NULL)
    return;
  nng_listener *xp = (nng_listener *) R_ExternalPtrAddr(xptr);
  R_Free(xp);
  R_ClearExternalPtr(xptr);

}

/* contexts ----------------------------------------------------------------- */

SEXP rnng_ctx_open(SEXP socket) {

  if (R_ExternalPtrTag(socket) != nano_SocketSymbol)
    error_return("'socket' is not a valid Socket");
  nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(socket);
  nng_ctx *ctxp = R_Calloc(1, nng_ctx);
  int xc = nng_ctx_open(ctxp, *sock);
  if (xc)
    return Rf_ScalarInteger(xc);
  SEXP context = PROTECT(R_MakeExternalPtr(ctxp, nano_ContextSymbol, R_NilValue));
  R_RegisterCFinalizerEx(context, context_finalizer, TRUE);
  SEXP klass = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_STRING_ELT(klass, 0, Rf_mkChar("nanoContext"));
  SET_STRING_ELT(klass, 1, Rf_mkChar("nano"));
  Rf_classgets(context, klass);
  int id = nng_ctx_id(*ctxp);
  Rf_setAttrib(context, nano_IdSymbol, Rf_ScalarInteger(id));
  Rf_setAttrib(context, nano_StateSymbol, Rf_mkString("opened"));
  Rf_setAttrib(context, nano_ProtocolSymbol, Rf_getAttrib(socket, nano_ProtocolSymbol));
  int sid = nng_socket_id(*sock);
  Rf_setAttrib(context, nano_SocketSymbol, Rf_ScalarInteger(sid));

  UNPROTECT(2);
  return(context);

}

SEXP rnng_ctx_close(SEXP context) {

  if (R_ExternalPtrTag(context) != nano_ContextSymbol)
    error_return("'context' is not a valid Context");
  nng_ctx *ctxp = (nng_ctx *) R_ExternalPtrAddr(context);
  int xc = nng_ctx_close(*ctxp);
  if (!xc)
    Rf_setAttrib(context, nano_StateSymbol, Rf_mkString("closed"));
  return Rf_ScalarInteger(xc);

}

/* dialers and listeners ---------------------------------------------------- */

SEXP rnng_dial(SEXP socket, SEXP url) {

  if (R_ExternalPtrTag(socket) != nano_SocketSymbol)
    error_return("'socket' is not a valid Socket");
  nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(socket);
  const char *up = CHAR(STRING_ELT(url, 0));
  nng_dialer *dp = R_Calloc(1, nng_dialer);
  int xc = nng_dial(*sock, up, dp, 2u);
  if (xc)
    return Rf_ScalarInteger(xc);
  SEXP dialer = PROTECT(R_MakeExternalPtr(dp, nano_DialerSymbol, R_NilValue));
  R_RegisterCFinalizerEx(dialer, dialer_finalizer, TRUE);
  SEXP klass = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_STRING_ELT(klass, 0, Rf_mkChar("nanoDialer"));
  SET_STRING_ELT(klass, 1, Rf_mkChar("nano"));
  Rf_classgets(dialer, klass);
  int id = nng_dialer_id(*dp);
  Rf_setAttrib(dialer, nano_IdSymbol, Rf_ScalarInteger(id));
  Rf_setAttrib(dialer, nano_UrlSymbol, url);
  Rf_setAttrib(dialer, nano_StateSymbol, Rf_mkString("started"));
  int sid = nng_socket_id(*sock);
  Rf_setAttrib(dialer, nano_SocketSymbol, Rf_ScalarInteger(sid));

  UNPROTECT(2);
  return dialer;

}

SEXP rnng_dialer_create(SEXP socket, SEXP url) {

  if (R_ExternalPtrTag(socket) != nano_SocketSymbol)
    error_return("'socket' is not a valid Socket");
  nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(socket);
  const char *up = CHAR(STRING_ELT(url, 0));
  nng_dialer *dp = R_Calloc(1, nng_dialer);
  int xc = nng_dialer_create(dp, *sock, up);
  if (xc)
    return Rf_ScalarInteger(xc);
  SEXP dialer = PROTECT(R_MakeExternalPtr(dp, nano_DialerSymbol, R_NilValue));
  R_RegisterCFinalizerEx(dialer, dialer_finalizer, TRUE);
  SEXP klass = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_STRING_ELT(klass, 0, Rf_mkChar("nanoDialer"));
  SET_STRING_ELT(klass, 1, Rf_mkChar("nano"));
  Rf_classgets(dialer, klass);
  int id = nng_dialer_id(*dp);
  Rf_setAttrib(dialer, nano_IdSymbol, Rf_ScalarInteger(id));
  Rf_setAttrib(dialer, nano_UrlSymbol, url);
  Rf_setAttrib(dialer, nano_StateSymbol, Rf_mkString("not started"));
  int sid = nng_socket_id(*sock);
  Rf_setAttrib(dialer, nano_SocketSymbol, Rf_ScalarInteger(sid));

  UNPROTECT(2);
  return dialer;

}

SEXP rnng_listen(SEXP socket, SEXP url) {

  if (R_ExternalPtrTag(socket) != nano_SocketSymbol)
    error_return("'socket' is not a valid Socket");
  nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(socket);
  const char *up = CHAR(STRING_ELT(url, 0));
  nng_listener *lp = R_Calloc(1, nng_listener);
  int xc = nng_listen(*sock, up, lp, 0);
  if (xc)
    return Rf_ScalarInteger(xc);
  SEXP listener = PROTECT(R_MakeExternalPtr(lp, nano_ListenerSymbol, R_NilValue));
  R_RegisterCFinalizerEx(listener, listener_finalizer, TRUE);
  SEXP klass = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_STRING_ELT(klass, 0, Rf_mkChar("nanoListener"));
  SET_STRING_ELT(klass, 1, Rf_mkChar("nano"));
  Rf_classgets(listener, klass);
  int id = nng_listener_id(*lp);
  Rf_setAttrib(listener, nano_IdSymbol, Rf_ScalarInteger(id));
  Rf_setAttrib(listener, nano_UrlSymbol, url);
  Rf_setAttrib(listener, nano_StateSymbol, Rf_mkString("started"));
  int sid = nng_socket_id(*sock);
  Rf_setAttrib(listener, nano_SocketSymbol, Rf_ScalarInteger(sid));

  UNPROTECT(2);
  return listener;

}

SEXP rnng_listener_create(SEXP socket, SEXP url) {

  if (R_ExternalPtrTag(socket) != nano_SocketSymbol)
    error_return("'socket' is not a valid Socket");
  nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(socket);
  const char *up = CHAR(STRING_ELT(url, 0));
  nng_listener *lp = R_Calloc(1, nng_listener);
  int xc = nng_listener_create(lp, *sock, up);
  if (xc)
    return Rf_ScalarInteger(xc);
  SEXP listener = PROTECT(R_MakeExternalPtr(lp, nano_ListenerSymbol, R_NilValue));
  R_RegisterCFinalizerEx(listener, listener_finalizer, TRUE);
  SEXP klass = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_STRING_ELT(klass, 0, Rf_mkChar("nanoListener"));
  SET_STRING_ELT(klass, 1, Rf_mkChar("nano"));
  Rf_classgets(listener, klass);
  int id = nng_listener_id(*lp);
  Rf_setAttrib(listener, nano_IdSymbol, Rf_ScalarInteger(id));
  Rf_setAttrib(listener, nano_UrlSymbol, url);
  Rf_setAttrib(listener, nano_StateSymbol, Rf_mkString("not started"));
  int sid = nng_socket_id(*sock);
  Rf_setAttrib(listener, nano_SocketSymbol, Rf_ScalarInteger(sid));

  UNPROTECT(2);
  return listener;

}

SEXP rnng_dialer_start(SEXP dialer, SEXP async) {

  if (R_ExternalPtrTag(dialer) != nano_DialerSymbol)
    error_return("'dialer' is not a valid Dialer");
  nng_dialer *dial = (nng_dialer *) R_ExternalPtrAddr(dialer);
  const Rboolean asy = Rf_asLogical(async);
  int flags = asy == 0 ? 0 : 2u;
  int xc = nng_dialer_start(*dial, flags);
  if (!xc)
    Rf_setAttrib(dialer, nano_StateSymbol, Rf_mkString("started"));
  return Rf_ScalarInteger(xc);

}

SEXP rnng_listener_start(SEXP listener) {

  if (R_ExternalPtrTag(listener) != nano_ListenerSymbol)
    error_return("'listener' is not a valid Listener");
  nng_listener *list = (nng_listener *) R_ExternalPtrAddr(listener);
  int xc = nng_listener_start(*list, 0);
  if (!xc)
    Rf_setAttrib(listener, nano_StateSymbol, Rf_mkString("started"));
  return Rf_ScalarInteger(xc);

}

SEXP rnng_dialer_close(SEXP dialer) {

  if (R_ExternalPtrTag(dialer) != nano_DialerSymbol)
    error_return("'dialer' is not a valid Dialer");
  nng_dialer *dial = (nng_dialer *) R_ExternalPtrAddr(dialer);
  int xc = nng_dialer_close(*dial);
  if (!xc)
    Rf_setAttrib(dialer, nano_StateSymbol, Rf_mkString("closed"));
  return Rf_ScalarInteger(xc);

}

SEXP rnng_listener_close(SEXP listener) {

  if (R_ExternalPtrTag(listener) != nano_ListenerSymbol)
    error_return("'listener' is not a valid listener");
  nng_listener *list = (nng_listener *) R_ExternalPtrAddr(listener);
  int xc = nng_listener_close(*list);
  if (!xc)
    Rf_setAttrib(listener, nano_StateSymbol, Rf_mkString("closed"));
  return Rf_ScalarInteger(xc);

}

/* send and recv ------------------------------------------------------------ */
/* nng flags: bitmask of NNG_FLAG_ALLOC = 1u + NNG_FLAG_NONBLOCK = 2u ------- */

SEXP rnng_send(SEXP socket, SEXP data, SEXP block, SEXP echo) {

  if (R_ExternalPtrTag(socket) != nano_SocketSymbol)
    error_return("'socket' is not a valid Socket");
  nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(socket);
  const Rboolean ech = Rf_asLogical(echo);
  const Rboolean blk = Rf_asLogical(block);
  int flags = blk == 1 ? 0 : 2u;
  const R_xlen_t dlen = XLENGTH(data);
  void *dp = (void *) RAW(data);
  int xc =  nng_send(*sock, dp, dlen, flags);

  if (xc)
    return Rf_ScalarInteger(xc);
  if (ech != 0)
    return(data);
  return R_NilValue;

}

SEXP rnng_recv(SEXP socket, SEXP block) {

  if (R_ExternalPtrTag(socket) != nano_SocketSymbol)
    error_return("'socket' is not a valid Socket");
  nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(socket);
  const Rboolean blk = Rf_asLogical(block);
  int flags = blk == 1 ? 1u : 3u;
  char *buf = NULL;
  size_t sz;
  int xc = nng_recv(*sock, &buf, &sz, flags);
  if (xc)
    return Rf_ScalarInteger(xc);

  SEXP res = PROTECT(Rf_allocVector(RAWSXP, sz));
  unsigned char *rp = RAW(res);
  memcpy(rp, buf, sz);
  nng_free(buf, sz);
  UNPROTECT(1);
  return(res);

}

/* NNG_DURATION_INFINITE (-1) NNG_DURATION_DEFAULT (-2) NNG_DURATION_ZERO (0) */

SEXP rnng_send_aio(SEXP socket, SEXP data, SEXP timeout) {

  if (R_ExternalPtrTag(socket) != nano_SocketSymbol)
    error_return("'socket' is not a valid Socket");
  nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(socket);
  const nng_duration dur = (nng_duration) Rf_asInteger(timeout);
  const R_xlen_t dlen = XLENGTH(data);
  nng_msg *msgp[dlen];
  nng_aio *aiop[dlen];

  for (R_xlen_t i = 0; i < dlen; i++) {
    SEXP vec = VECTOR_ELT(data, i);
    const R_xlen_t vlen = XLENGTH(vec);
    void *dp = (void *) RAW(vec);
    int xc = nng_msg_alloc(&msgp[i], 0);
    if (xc)
      return Rf_ScalarInteger(xc);
    xc = nng_msg_append(msgp[i], dp, vlen);
    if (xc)
      return Rf_ScalarInteger(xc);

    xc = nng_aio_alloc(&aiop[i], NULL, NULL);
    if (xc)
      return Rf_ScalarInteger(xc);
    nng_aio_set_msg(aiop[i], msgp[i]);
    nng_aio_set_timeout(aiop[i], dur);
    nng_send_aio(*sock, aiop[i]);
  }

  SEXP out = PROTECT(Rf_allocVector(INTSXP, dlen));
  for (R_xlen_t i = 0; i < dlen; i++) {
    nng_aio_wait(aiop[i]);
    int xc = nng_aio_result(aiop[i]);
    if (xc) {
      nng_msg *dmsg = nng_aio_get_msg(aiop[i]);
      nng_msg_free(dmsg);
    }
    nng_aio_free(aiop[i]);
    SET_INTEGER_ELT(out, i, xc);
  }

  UNPROTECT(1);
  return out;

}

SEXP rnng_recv_aio(SEXP socket, SEXP n, SEXP timeout) {

  if (R_ExternalPtrTag(socket) != nano_SocketSymbol)
    error_return("'socket' is not a valid Socket");
  nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(socket);
  const R_xlen_t dlen = Rf_asInteger(n);
  const nng_duration dur = (nng_duration) Rf_asInteger(timeout);
  nng_aio *aiop[dlen];

  for (R_xlen_t i = 0; i < dlen; i++) {
    int xc = nng_aio_alloc(&aiop[i], NULL, NULL);
    if (xc)
      return Rf_ScalarInteger(xc);
    nng_aio_set_timeout(aiop[i], dur);
    nng_recv_aio(*sock, aiop[i]);
  }

  SEXP res = PROTECT(Rf_allocVector(VECSXP, dlen));
  for (R_xlen_t i = 0; i < dlen; i++) {
    nng_aio_wait(aiop[i]);
    int xc = nng_aio_result(aiop[i]);
    if (xc) {
      nng_aio_free(aiop[i]);
      SET_VECTOR_ELT(res, i, Rf_ScalarInteger(xc));
      continue;
    }
    nng_msg *msgp = nng_aio_get_msg(aiop[i]);
    size_t sz = nng_msg_len(msgp);
    SEXP item = PROTECT(Rf_allocVector(RAWSXP, sz));
    SET_VECTOR_ELT(res, i, item);
    unsigned char *rp = RAW(item);
    memcpy(rp, nng_msg_body(msgp), sz);
    nng_msg_free(msgp);
    nng_aio_free(aiop[i]);
    UNPROTECT(1);
  }

  UNPROTECT(1);
  return(res);

}

SEXP rnng_ctx_send(SEXP context, SEXP data, SEXP timeout) {

  if (R_ExternalPtrTag(context) != nano_ContextSymbol)
    error_return("'context' is not a valid Context");
  nng_ctx *ctxp = (nng_ctx *) R_ExternalPtrAddr(context);
  const nng_duration dur = (nng_duration) Rf_asInteger(timeout);
  const R_xlen_t dlen = XLENGTH(data);
  nng_msg *msgp[dlen];
  nng_aio *aiop[dlen];

  for (R_xlen_t i = 0; i < dlen; i++) {
    SEXP vec = VECTOR_ELT(data, i);
    const R_xlen_t vlen = XLENGTH(vec);
    void *dp = (void *) RAW(vec);
    int xc = nng_msg_alloc(&msgp[i], 0);
    if (xc)
      return Rf_ScalarInteger(xc);
    xc = nng_msg_append(msgp[i], dp, vlen);
    if (xc)
      return Rf_ScalarInteger(xc);

    xc = nng_aio_alloc(&aiop[i], NULL, NULL);
    if (xc)
      return Rf_ScalarInteger(xc);
    nng_aio_set_msg(aiop[i], msgp[i]);
    nng_aio_set_timeout(aiop[i], dur);
    nng_ctx_send(*ctxp, aiop[i]);
  }

  SEXP out = PROTECT(Rf_allocVector(INTSXP, dlen));
  for (R_xlen_t i = 0; i < dlen; i++) {
    nng_aio_wait(aiop[i]);
    int xc = nng_aio_result(aiop[i]);
    if (xc) {
      nng_msg *dmsg = nng_aio_get_msg(aiop[i]);
      nng_msg_free(dmsg);
    }
    nng_aio_free(aiop[i]);
    SET_INTEGER_ELT(out, i, xc);
  }

  UNPROTECT(1);
  return out;

}

SEXP rnng_ctx_recv(SEXP context, SEXP n, SEXP timeout) {

  if (R_ExternalPtrTag(context) != nano_ContextSymbol)
    error_return("'context' is not a valid Context");
  nng_ctx *ctxp = (nng_ctx *) R_ExternalPtrAddr(context);
  const R_xlen_t dlen = Rf_asInteger(n);
  const nng_duration dur = (nng_duration) Rf_asInteger(timeout);
  nng_aio *aiop[dlen];

  for (R_xlen_t i = 0; i < dlen; i++) {
    int xc = nng_aio_alloc(&aiop[i], NULL, NULL);
    if (xc)
      return Rf_ScalarInteger(xc);
    nng_aio_set_timeout(aiop[i], dur);
    nng_ctx_recv(*ctxp, aiop[i]);
  }

  SEXP res = PROTECT(Rf_allocVector(VECSXP, dlen));
  for (R_xlen_t i = 0; i < dlen; i++) {
    nng_aio_wait(aiop[i]);
    int xc = nng_aio_result(aiop[i]);
    if (xc) {
      nng_aio_free(aiop[i]);
      SET_VECTOR_ELT(res, i, Rf_ScalarInteger(xc));
      continue;
    }
    nng_msg *msgp = nng_aio_get_msg(aiop[i]);
    size_t sz = nng_msg_len(msgp);
    SEXP item = PROTECT(Rf_allocVector(RAWSXP, sz));
    SET_VECTOR_ELT(res, i, item);
    unsigned char *rp = RAW(item);
    memcpy(rp, nng_msg_body(msgp), sz);
    nng_msg_free(msgp);
    nng_aio_free(aiop[i]);
    UNPROTECT(1);
  }

  UNPROTECT(1);
  return(res);

}

