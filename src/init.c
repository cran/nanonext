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

// nanonext - package level registrations --------------------------------------

#include "nanonext.h"

SEXP nano_AioSymbol;
SEXP nano_ContextSymbol;
SEXP nano_CvSymbol;
SEXP nano_DataSymbol;
SEXP nano_DialerSymbol;
SEXP nano_DotcallSymbol;
SEXP nano_HeadersSymbol;
SEXP nano_IdSymbol;
SEXP nano_ListenerSymbol;
SEXP nano_NewEnvSymbol;
SEXP nano_PipeSymbol;
SEXP nano_ProtocolSymbol;
SEXP nano_RawSymbol;
SEXP nano_ResponseSymbol;
SEXP nano_ResultSymbol;
SEXP nano_RtcSymbol;
SEXP nano_SerialSymbol;
SEXP nano_SessionSymbol;
SEXP nano_SocketSymbol;
SEXP nano_StateSymbol;
SEXP nano_StatusSymbol;
SEXP nano_StreamSymbol;
SEXP nano_TextframesSymbol;
SEXP nano_UnserSymbol;
SEXP nano_UrlSymbol;

SEXP nano_addRedirect;
SEXP nano_aioFormals;
SEXP nano_aioFuncs;
SEXP nano_aioNFuncs;
SEXP nano_error;
SEXP nano_ncurlAio;
SEXP nano_ncurlSession;
SEXP nano_recvAio;
SEXP nano_sendAio;
SEXP nano_success;
SEXP nano_unresolved;

static void RegisterSymbols(void) {
  nano_AioSymbol = Rf_install("aio");
  nano_ContextSymbol = Rf_install("context");
  nano_CvSymbol = Rf_install("cv");
  nano_DataSymbol = Rf_install("data");
  nano_DialerSymbol = Rf_install("dialer");
  nano_DotcallSymbol = Rf_install(".Call");
  nano_HeadersSymbol = Rf_install("headers");
  nano_IdSymbol = Rf_install("id");
  nano_ListenerSymbol = Rf_install("listener");
  nano_NewEnvSymbol = Rf_install("new.env");
  nano_PipeSymbol = Rf_install("pipe");
  nano_ProtocolSymbol = Rf_install("protocol");
  nano_RawSymbol = Rf_install("raw");
  nano_ResponseSymbol = Rf_install("response");
  nano_ResultSymbol = Rf_install("result");
  nano_RtcSymbol = Rf_install("rawToChar");
  nano_SerialSymbol = Rf_install("serialize");
  nano_SessionSymbol = Rf_install("session");
  nano_SocketSymbol = Rf_install("socket");
  nano_StateSymbol = Rf_install("state");
  nano_StatusSymbol = Rf_install("status");
  nano_StreamSymbol = Rf_install("stream");
  nano_TextframesSymbol = Rf_install("textframes");
  nano_UnserSymbol = Rf_install("unserialize");
  nano_UrlSymbol = Rf_install("url");
}

static void PreserveObjects(void) {
  R_PreserveObject(nano_addRedirect = Rf_allocVector(STRSXP, 1));
  R_PreserveObject(nano_aioFormals = Rf_cons(nano_AioSymbol, R_NilValue));
  R_PreserveObject(nano_aioFuncs = Rf_allocVector(LISTSXP, 3));
  SETCAR(nano_aioFuncs, Rf_lang3(nano_DotcallSymbol, Rf_install("rnng_aio_result"), nano_DataSymbol));
  SETCADR(nano_aioFuncs, Rf_lang3(nano_DotcallSymbol, Rf_install("rnng_aio_get_msgdata"), nano_DataSymbol));
  SETCADDR(nano_aioFuncs, Rf_lang3(nano_DotcallSymbol, Rf_install("rnng_aio_get_msgraw"), nano_DataSymbol));
  SEXP nano_AioHttpSymbol = Rf_install("rnng_aio_http");
  R_PreserveObject(nano_aioNFuncs = Rf_allocVector(LISTSXP, 4));
  SETCAR(nano_aioNFuncs, Rf_lang5(nano_DotcallSymbol, nano_AioHttpSymbol, nano_DataSymbol, nano_ResponseSymbol, Rf_ScalarInteger(1)));
  SETCADR(nano_aioNFuncs, Rf_lang5(nano_DotcallSymbol, nano_AioHttpSymbol, nano_DataSymbol, nano_ResponseSymbol, Rf_ScalarInteger(2)));
  SETCADDR(nano_aioNFuncs, Rf_lang5(nano_DotcallSymbol, nano_AioHttpSymbol, nano_DataSymbol, nano_ResponseSymbol, Rf_ScalarInteger(3)));
  SETCADDDR(nano_aioNFuncs, Rf_lang5(nano_DotcallSymbol, nano_AioHttpSymbol, nano_DataSymbol, nano_ResponseSymbol, Rf_ScalarInteger(4)));
  R_PreserveObject(nano_error = Rf_cons(Rf_mkString("errorValue"), R_NilValue));
  SET_TAG(nano_error, R_ClassSymbol);
  R_PreserveObject(nano_ncurlAio = Rf_allocVector(STRSXP, 2));
  SET_STRING_ELT(nano_ncurlAio, 0, Rf_mkChar("ncurlAio"));
  SET_STRING_ELT(nano_ncurlAio, 1, Rf_mkChar("recvAio"));
  R_PreserveObject(nano_ncurlSession = Rf_mkString("ncurlSession"));
  R_PreserveObject(nano_recvAio = Rf_mkString("recvAio"));
  R_PreserveObject(nano_sendAio = Rf_mkString("sendAio"));
  R_PreserveObject(nano_success = Rf_ScalarInteger(0));
  R_PreserveObject(nano_unresolved = Rf_shallow_duplicate(Rf_ScalarLogical(NA_LOGICAL)));
  Rf_classgets(nano_unresolved, Rf_mkString("unresolvedValue"));
}

static void ReleaseObjects(void) {
  R_ReleaseObject(nano_unresolved);
  R_ReleaseObject(nano_success);
  R_ReleaseObject(nano_sendAio);
  R_ReleaseObject(nano_recvAio);
  R_ReleaseObject(nano_ncurlSession);
  R_ReleaseObject(nano_ncurlAio);
  R_ReleaseObject(nano_error);
  R_ReleaseObject(nano_aioNFuncs);
  R_ReleaseObject(nano_aioFuncs);
  R_ReleaseObject(nano_aioFormals);
  R_ReleaseObject(nano_addRedirect);
}

static const R_CallMethodDef callMethods[] = {
  {"rnng_aio_call", (DL_FUNC) &rnng_aio_call, 1},
  {"rnng_aio_get_msgdata", (DL_FUNC) &rnng_aio_get_msgdata, 1},
  {"rnng_aio_get_msgraw", (DL_FUNC) &rnng_aio_get_msgraw, 1},
  {"rnng_aio_http", (DL_FUNC) &rnng_aio_http, 3},
  {"rnng_aio_result", (DL_FUNC) &rnng_aio_result, 1},
  {"rnng_aio_stop", (DL_FUNC) &rnng_aio_stop, 1},
  {"rnng_clock", (DL_FUNC) &rnng_clock, 0},
  {"rnng_close", (DL_FUNC) &rnng_close, 1},
  {"rnng_ctx_close", (DL_FUNC) &rnng_ctx_close, 1},
  {"rnng_ctx_open", (DL_FUNC) &rnng_ctx_open, 1},
  {"rnng_cv_alloc", (DL_FUNC) &rnng_cv_alloc, 0},
  {"rnng_cv_recv_aio", (DL_FUNC) &rnng_cv_recv_aio, 7},
  {"rnng_cv_request", (DL_FUNC) &rnng_cv_request, 8},
  {"rnng_cv_reset", (DL_FUNC) &rnng_cv_reset, 1},
  {"rnng_cv_until", (DL_FUNC) &rnng_cv_until, 2},
  {"rnng_cv_value", (DL_FUNC) &rnng_cv_value, 1},
  {"rnng_cv_wait", (DL_FUNC) &rnng_cv_wait, 1},
  {"rnng_device", (DL_FUNC) &rnng_device, 2},
  {"rnng_dial", (DL_FUNC) &rnng_dial, 4},
  {"rnng_dialer_close", (DL_FUNC) &rnng_dialer_close, 1},
  {"rnng_dialer_start", (DL_FUNC) &rnng_dialer_start, 2},
  {"rnng_base64dec", (DL_FUNC) &rnng_base64dec, 2},
  {"rnng_base64enc", (DL_FUNC) &rnng_base64enc, 2},
  {"rnng_get_opt", (DL_FUNC) &rnng_get_opt, 2},
  {"rnng_is_nul_byte", (DL_FUNC) &rnng_is_nul_byte, 1},
  {"rnng_listen", (DL_FUNC) &rnng_listen, 4},
  {"rnng_listener_close", (DL_FUNC) &rnng_listener_close, 1},
  {"rnng_listener_start", (DL_FUNC) &rnng_listener_start, 1},
  {"rnng_make_weakref", (DL_FUNC) &rnng_make_weakref, 2},
  {"rnng_messenger", (DL_FUNC) &rnng_messenger, 1},
  {"rnng_messenger_thread_create", (DL_FUNC) &rnng_messenger_thread_create, 1},
  {"rnng_msg_pipe", (DL_FUNC) &rnng_msg_pipe, 1},
  {"rnng_ncurl", (DL_FUNC) &rnng_ncurl, 8},
  {"rnng_ncurl_aio", (DL_FUNC) &rnng_ncurl_aio, 7},
  {"rnng_ncurl_session", (DL_FUNC) &rnng_ncurl_session, 8},
  {"rnng_ncurl_session_close", (DL_FUNC) &rnng_ncurl_session_close, 1},
  {"rnng_ncurl_transact", (DL_FUNC) &rnng_ncurl_transact, 1},
  {"rnng_pipe_close", (DL_FUNC) &rnng_pipe_close, 1},
  {"rnng_pipe_notify", (DL_FUNC) &rnng_pipe_notify, 6},
  {"rnng_protocol_open", (DL_FUNC) &rnng_protocol_open, 2},
  {"rnng_random", (DL_FUNC) &rnng_random, 1},
  {"rnng_recv", (DL_FUNC) &rnng_recv, 5},
  {"rnng_recv_aio", (DL_FUNC) &rnng_recv_aio, 6},
  {"rnng_request", (DL_FUNC) &rnng_request, 7},
  {"rnng_send", (DL_FUNC) &rnng_send, 4},
  {"rnng_send_aio", (DL_FUNC) &rnng_send_aio, 5},
  {"rnng_set_opt", (DL_FUNC) &rnng_set_opt, 3},
  {"rnng_sha1", (DL_FUNC) &rnng_sha1, 3},
  {"rnng_sha224", (DL_FUNC) &rnng_sha224, 3},
  {"rnng_sha256", (DL_FUNC) &rnng_sha256, 3},
  {"rnng_sha384", (DL_FUNC) &rnng_sha384, 3},
  {"rnng_sha512", (DL_FUNC) &rnng_sha512, 3},
  {"rnng_sleep", (DL_FUNC) &rnng_sleep, 1},
  {"rnng_stats_get", (DL_FUNC) &rnng_stats_get, 2},
  {"rnng_status_code", (DL_FUNC) &rnng_status_code, 1},
  {"rnng_stream_close", (DL_FUNC) &rnng_stream_close, 1},
  {"rnng_stream_dial", (DL_FUNC) &rnng_stream_dial, 3},
  {"rnng_stream_listen", (DL_FUNC) &rnng_stream_listen, 3},
  {"rnng_strerror", (DL_FUNC) &rnng_strerror, 1},
  {"rnng_subscribe", (DL_FUNC) &rnng_subscribe, 3},
  {"rnng_unresolved", (DL_FUNC) &rnng_unresolved, 1},
  {"rnng_unresolved2", (DL_FUNC) &rnng_unresolved2, 1},
  {"rnng_url_parse", (DL_FUNC) &rnng_url_parse, 1},
  {"rnng_version", (DL_FUNC) &rnng_version, 0},
  {NULL, NULL, 0}
};

void attribute_visible R_init_nanonext(DllInfo* dll) {
  RegisterSymbols();
  PreserveObjects();
  R_registerRoutines(dll, NULL, callMethods, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);
  R_forceSymbols(dll, TRUE);
}

void attribute_visible R_unload_nanonext(DllInfo *info) {
  ReleaseObjects();
}

