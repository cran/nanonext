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

// nanonext - C level - Core Functions -----------------------------------------

#define NANONEXT_SUPPLEMENTALS
#include "nanonext.h"

// definitions and statics -----------------------------------------------------

typedef enum nano_aio_typ {
  SENDAIO,
  RECVAIO,
  IOV_SENDAIO,
  IOV_RECVAIO,
  HTTP_AIO
} nano_aio_typ;

typedef struct nano_aio_s {
  nng_aio *aio;
  nano_aio_typ type;
  int mode;
  int result;
  void *data;
} nano_aio;

typedef struct nano_handle_s {
  nng_url *url;
  nng_http_client *cli;
  nng_http_req *req;
  nng_http_res *res;
  nng_tls_config *cfg;
} nano_handle;

typedef struct nano_cv_aio_s {
  nano_cv *cv;
  nano_aio *aio;
} nano_cv_aio;

typedef struct nano_cv_duo_s {
  nano_cv *cv;
  nano_cv *cv2;
} nano_cv_duo;

static void pipe_cb_signal_cv(nng_pipe p, nng_pipe_ev ev, void *arg) {

  nano_cv *ncv = (nano_cv *) arg;
  nng_cv *cv = ncv->cv;
  nng_mtx *mtx = ncv->mtx;

  nng_mtx_lock(mtx);
  ncv->condition++;
  nng_cv_wake(cv);
  nng_mtx_unlock(mtx);

}

static void pipe_cb_signal_cv_duo(nng_pipe p, nng_pipe_ev ev, void *arg) {

  nano_cv_duo *duo = (nano_cv_duo *) arg;
  nano_cv *ncv = duo->cv;
  nano_cv *ncv2 = duo->cv2;
  nng_cv *cv = ncv->cv;
  nng_cv *cv2 = ncv2->cv;
  nng_mtx *mtx = ncv->mtx;
  nng_mtx *mtx2 = ncv2->mtx;

  nng_mtx_lock(mtx);
  ncv->condition++;
  nng_cv_wake(cv);
  nng_mtx_unlock(mtx);

  nng_mtx_lock(mtx2);
  ncv2->condition++;
  nng_cv_wake(cv2);
  nng_mtx_unlock(mtx2);

}

static void pipe_cb_flag_cv(nng_pipe p, nng_pipe_ev ev, void *arg) {

  nano_cv *ncv = (nano_cv *) arg;
  nng_cv *cv = ncv->cv;
  nng_mtx *mtx = ncv->mtx;

  nng_mtx_lock(mtx);
  ncv->flag = 1;
  ncv->condition++;
  nng_cv_wake(cv);
  nng_mtx_unlock(mtx);

}

static void pipe_cb_flag_cv_duo(nng_pipe p, nng_pipe_ev ev, void *arg) {

  nano_cv_duo *duo = (nano_cv_duo *) arg;
  nano_cv *ncv = duo->cv;
  nano_cv *ncv2 = duo->cv2;
  nng_cv *cv = ncv->cv;
  nng_cv *cv2 = ncv2->cv;
  nng_mtx *mtx = ncv->mtx;
  nng_mtx *mtx2 = ncv2->mtx;

  nng_mtx_lock(mtx);
  ncv->flag = 1;
  ncv->condition++;
  nng_cv_wake(cv);
  nng_mtx_unlock(mtx);

  nng_mtx_lock(mtx2);
  ncv2->flag = 1;
  ncv2->condition++;
  nng_cv_wake(cv2);
  nng_mtx_unlock(mtx2);

}

static void pipe_cb_dropcon(nng_pipe p, nng_pipe_ev ev, void *arg) {

  if (arg != NULL) {
    nano_cv *ncv = (nano_cv *) arg;
    if (ncv->condition)
      nng_pipe_close(p);
  } else {
    nng_pipe_close(p);
  }

}

static void saio_complete(void *arg) {

  nano_aio *saio = (nano_aio *) arg;
  const int res = nng_aio_result(saio->aio);
  if (res)
    nng_msg_free(nng_aio_get_msg(saio->aio));

#if NNG_MAJOR_VERSION == 1 && NNG_MINOR_VERSION < 6
  nng_mtx_lock(shr_mtx);
  saio->result = res ? res : -1;
  nng_mtx_unlock(shr_mtx);
#else
  saio->result = res ? res : -1;
#endif

}

static void isaio_complete(void *arg) {

  nano_aio *iaio = (nano_aio *) arg;
  const int res = nng_aio_result(iaio->aio);
  if (iaio->data != NULL)
    R_Free(iaio->data);

#if NNG_MAJOR_VERSION == 1 && NNG_MINOR_VERSION < 6
  nng_mtx_lock(shr_mtx);
  iaio->result = res ? res : -1;
  nng_mtx_unlock(shr_mtx);
#else
  iaio->result = res ? res : -1;
#endif

}

static void raio_complete(void *arg) {

  nano_aio *raio = (nano_aio *) arg;
  const int res = nng_aio_result(raio->aio);
  if (res == 0)
    raio->data = nng_aio_get_msg(raio->aio);

#if NNG_MAJOR_VERSION == 1 && NNG_MINOR_VERSION < 6
  nng_mtx_lock(shr_mtx);
  raio->result = res ? res : -1;
  nng_mtx_unlock(shr_mtx);
#else
  raio->result = res ? res : -1;
#endif

}

static void raio_complete_signal(void *arg) {

  nano_cv_aio *cv_aio = (nano_cv_aio *) arg;
  nano_aio *aio = cv_aio->aio;
  nano_cv *ncv = cv_aio->cv;
  nng_cv *cv = ncv->cv;
  nng_mtx *mtx = ncv->mtx;

  const int res = nng_aio_result(aio->aio);
  if (res == 0)
    aio->data = nng_aio_get_msg(aio->aio);

  nng_mtx_lock(mtx);
  aio->result = res ? res : -1;
  ncv->condition++;
  nng_cv_wake(cv);
  nng_mtx_unlock(mtx);

}

static void iraio_complete(void *arg) {

  nano_aio *iaio = (nano_aio *) arg;
  const int res = nng_aio_result(iaio->aio);

#if NNG_MAJOR_VERSION == 1 && NNG_MINOR_VERSION < 6
  nng_mtx_lock(shr_mtx);
  iaio->result = res ? res : -1;
  nng_mtx_unlock(shr_mtx);
#else
  iaio->result = res ? res : -1;
#endif

}

static void iraio_complete_signal(void *arg) {

  nano_cv_aio *cv_aio = (nano_cv_aio *) arg;
  nano_aio *aio = cv_aio->aio;
  nano_cv *ncv = cv_aio->cv;
  nng_cv *cv = ncv->cv;
  nng_mtx *mtx = ncv->mtx;

  const int res = nng_aio_result(aio->aio);

  nng_mtx_lock(mtx);
  aio->result = res ? res : -1;
  ncv->condition++;
  nng_cv_wake(cv);
  nng_mtx_unlock(mtx);

}

static void saio_finalizer(SEXP xptr) {

  if (R_ExternalPtrAddr(xptr) == NULL)
    return;
  nano_aio *xp = (nano_aio *) R_ExternalPtrAddr(xptr);
  nng_aio_free(xp->aio);
  R_Free(xp);

}

static void raio_finalizer(SEXP xptr) {

  if (R_ExternalPtrAddr(xptr) == NULL)
    return;
  nano_aio *xp = (nano_aio *) R_ExternalPtrAddr(xptr);
  nng_aio_free(xp->aio);
  if (xp->data != NULL)
    nng_msg_free((nng_msg *) xp->data);
  R_Free(xp);

}

static void cv_finalizer(SEXP xptr) {

  if (R_ExternalPtrAddr(xptr) == NULL)
    return;
  nano_cv *xp = (nano_cv *) R_ExternalPtrAddr(xptr);
  nng_cv_free(xp->cv);
  nng_mtx_free(xp->mtx);
  R_Free(xp);

}

static void reqsaio_finalizer(SEXP xptr) {

  if (R_ExternalPtrAddr(xptr) == NULL)
    return;
  nng_ctx *ctx = (nng_ctx *) R_ExternalPtrAddr(Rf_getAttrib(xptr, nano_ContextSymbol));
  nng_ctx_close(*ctx);
  nano_aio *xp = (nano_aio *) R_ExternalPtrAddr(xptr);
  nng_aio_free(xp->aio);
  R_Free(xp);

}

static void cv_aio_finalizer(SEXP xptr) {

  if (R_ExternalPtrAddr(xptr) == NULL)
    return;
  nano_cv_aio *xp = (nano_cv_aio *) R_ExternalPtrAddr(xptr);
  R_Free(xp);

}

static void cv_duo_finalizer(SEXP xptr) {

  if (R_ExternalPtrAddr(xptr) == NULL)
    return;
  nano_cv_duo *xp = (nano_cv_duo *) R_ExternalPtrAddr(xptr);
  R_Free(xp);

}

static void iaio_finalizer(SEXP xptr) {

  if (R_ExternalPtrAddr(xptr) == NULL)
    return;
  nano_aio *xp = (nano_aio *) R_ExternalPtrAddr(xptr);
  nng_aio_free(xp->aio);
  if (xp->data != NULL)
    R_Free(xp->data);
  R_Free(xp);

}

static void haio_finalizer(SEXP xptr) {

  if (R_ExternalPtrAddr(xptr) == NULL)
    return;
  nano_aio *xp = (nano_aio *) R_ExternalPtrAddr(xptr);
  nano_handle *handle = (nano_handle *) xp->data;
  nng_aio_free(xp->aio);
  if (handle->cfg != NULL)
    nng_tls_config_free(handle->cfg);
  nng_http_res_free(handle->res);
  nng_http_req_free(handle->req);
  nng_http_client_free(handle->cli);
  nng_url_free(handle->url);
  R_Free(handle);
  R_Free(xp);

}

static void session_finalizer(SEXP xptr) {

  if (R_ExternalPtrAddr(xptr) == NULL)
    return;
  nng_http_conn *xp = (nng_http_conn *) R_ExternalPtrAddr(xptr);
  nng_http_conn_close(xp);

}

static SEXP mk_error_saio(const int xc, SEXP env) {

  SEXP err = PROTECT(Rf_ScalarInteger(xc));
  SET_ATTRIB(err, nano_error);
  SET_OBJECT(err, 1);
  Rf_defineVar(nano_ResultSymbol, err, ENCLOS(env));
  Rf_defineVar(nano_AioSymbol, R_NilValue, env);
  UNPROTECT(1);
  return err;

}

static SEXP mk_error_raio(const int xc, SEXP env) {

  SEXP err = PROTECT(Rf_ScalarInteger(xc));
  SET_ATTRIB(err, nano_error);
  SET_OBJECT(err, 1);
  Rf_defineVar(nano_RawSymbol, err, ENCLOS(env));
  Rf_defineVar(nano_ResultSymbol, err, ENCLOS(env));
  Rf_defineVar(nano_AioSymbol, R_NilValue, env);
  UNPROTECT(1);
  return err;

}

static SEXP mk_error_haio(const int xc, SEXP env) {

  SEXP err = PROTECT(Rf_ScalarInteger(xc));
  SET_ATTRIB(err, nano_error);
  SET_OBJECT(err, 1);
  Rf_defineVar(nano_StatusSymbol, err, ENCLOS(env));
  Rf_defineVar(nano_StateSymbol, err, ENCLOS(env));
  Rf_defineVar(nano_RawSymbol, err, ENCLOS(env));
  Rf_defineVar(nano_ResultSymbol, err, ENCLOS(env));
  Rf_defineVar(nano_AioSymbol, R_NilValue, env);
  UNPROTECT(1);
  return err;

}

static SEXP mk_error_data(const int xc) {

  const char *names[] = {xc < 0 ? "result" : "data", ""};
  SEXP out = PROTECT(Rf_mkNamed(VECSXP, names));
  SEXP err = Rf_ScalarInteger(abs(xc));
  SET_ATTRIB(err, nano_error);
  SET_OBJECT(err, 1);
  SET_VECTOR_ELT(out, 0, err);
  UNPROTECT(1);
  return out;

}

// core aio --------------------------------------------------------------------

SEXP rnng_aio_result(SEXP env) {

  const SEXP exist = Rf_findVarInFrame(ENCLOS(env), nano_ResultSymbol);
  if (exist != R_UnboundValue)
    return exist;

  const SEXP aio = Rf_findVarInFrame(env, nano_AioSymbol);
  if (R_ExternalPtrTag(aio) != nano_AioSymbol)
    Rf_error("object is not a valid or active Aio");

  nano_aio *saio = (nano_aio *) R_ExternalPtrAddr(aio);

#if NNG_MAJOR_VERSION == 1 && NNG_MINOR_VERSION < 6
  int res;
  nng_mtx_lock(shr_mtx);
  res = saio->result;
  nng_mtx_unlock(shr_mtx);
  if (res == 0)
#else
  if (nng_aio_busy(saio->aio))
#endif
    return nano_unresolved;

  if (saio->result > 0)
    return mk_error_saio(saio->result, env);

  Rf_defineVar(nano_ResultSymbol, nano_success, ENCLOS(env));
  Rf_defineVar(nano_AioSymbol, R_NilValue, env);
  return nano_success;

}

SEXP rnng_aio_get_msgraw(SEXP env) {

  const SEXP exist = Rf_findVarInFrame(ENCLOS(env), nano_RawSymbol);
  if (exist != R_UnboundValue)
    return exist;

  const SEXP aio = Rf_findVarInFrame(env, nano_AioSymbol);
  if (R_ExternalPtrTag(aio) != nano_AioSymbol)
    Rf_error("object is not a valid or active Aio");

  nano_aio *raio = (nano_aio *) R_ExternalPtrAddr(aio);

#if NNG_MAJOR_VERSION == 1 && NNG_MINOR_VERSION < 6
  int res;
  nng_mtx_lock(shr_mtx);
  res = raio->result;
  nng_mtx_unlock(shr_mtx);
  if (res == 0)
#else
  if (nng_aio_busy(raio->aio))
#endif
    return nano_unresolved;

  if (raio->result > 0)
    return mk_error_raio(raio->result, env);

  SEXP out;
  const int mod = -raio->mode, kpr = 1;
  unsigned char *buf;
  size_t sz;

  if (raio->type == IOV_RECVAIO) {
    buf = raio->data;
    sz = nng_aio_count(raio->aio);
  } else {
    nng_msg *msg = (nng_msg *) raio->data;
    buf = nng_msg_body(msg);
    sz = nng_msg_len(msg);
  }

  PROTECT(out = nano_decode(buf, sz, mod, kpr));
  Rf_defineVar(nano_RawSymbol, VECTOR_ELT(out, 0), ENCLOS(env));
  Rf_defineVar(nano_ResultSymbol, VECTOR_ELT(out, 1), ENCLOS(env));
  Rf_defineVar(nano_AioSymbol, R_NilValue, env);
  out = VECTOR_ELT(out, 0);

  UNPROTECT(1);
  return out;

}

SEXP rnng_aio_get_msgraw2(SEXP env) {

  const SEXP exist = Rf_findVarInFrame(ENCLOS(env), nano_RawSymbol);
  if (exist != R_UnboundValue)
    return exist;

  const SEXP aio = Rf_findVarInFrame(env, nano_AioSymbol);
  if (R_ExternalPtrTag(aio) != nano_AioSymbol)
    Rf_error("object is not a valid or active Aio");

  nano_aio *raio = (nano_aio *) R_ExternalPtrAddr(aio);

  int res;
  nano_cv_aio *ncva = (nano_cv_aio *) R_ExternalPtrAddr(Rf_getAttrib(aio, nano_CvSymbol));
  nng_mtx *mtx = ncva->cv->mtx;
  nng_mtx_lock(mtx);
  res = raio->result;
  nng_mtx_unlock(mtx);
  if (res == 0)
    return nano_unresolved;

  if (res > 0)
    return mk_error_raio(res, env);

  SEXP out;
  const int mod = -raio->mode, kpr = 1;
  unsigned char *buf;
  size_t sz;

  if (raio->type == IOV_RECVAIO) {
    buf = raio->data;
    sz = nng_aio_count(raio->aio);
  } else {
    nng_msg *msg = (nng_msg *) raio->data;
    buf = nng_msg_body(msg);
    sz = nng_msg_len(msg);
  }

  PROTECT(out = nano_decode(buf, sz, mod, kpr));
  Rf_defineVar(nano_RawSymbol, VECTOR_ELT(out, 0), ENCLOS(env));
  Rf_defineVar(nano_ResultSymbol, VECTOR_ELT(out, 1), ENCLOS(env));
  Rf_defineVar(nano_AioSymbol, R_NilValue, env);
  out = VECTOR_ELT(out, 0);

  UNPROTECT(1);
  return out;

}

SEXP rnng_aio_get_msgdata(SEXP env) {

  const SEXP exist = Rf_findVarInFrame(ENCLOS(env), nano_ResultSymbol);
  if (exist != R_UnboundValue)
    return exist;

  const SEXP aio = Rf_findVarInFrame(env, nano_AioSymbol);
  if (R_ExternalPtrTag(aio) != nano_AioSymbol)
    Rf_error("object is not a valid or active Aio");

  nano_aio *raio = (nano_aio *) R_ExternalPtrAddr(aio);

#if NNG_MAJOR_VERSION == 1 && NNG_MINOR_VERSION < 6
  int res;
  nng_mtx_lock(shr_mtx);
  res = raio->result;
  nng_mtx_unlock(shr_mtx);
  if (res == 0)
#else
  if (nng_aio_busy(raio->aio))
#endif
    return nano_unresolved;

  if (raio->result > 0)
    return mk_error_raio(raio->result, env);

  SEXP out;
  const int kpr = raio->mode > 0 ? 0 : 1, mod = kpr ? -raio->mode : raio->mode;
  unsigned char *buf;
  size_t sz;

  if (raio->type == IOV_RECVAIO) {
    buf = raio->data;
    sz = nng_aio_count(raio->aio);
  } else {
    nng_msg *msg = (nng_msg *) raio->data;
    buf = nng_msg_body(msg);
    sz = nng_msg_len(msg);
  }

  PROTECT(out = nano_decode(buf, sz, mod, kpr));
  if (kpr) {
    Rf_defineVar(nano_RawSymbol, VECTOR_ELT(out, 0), ENCLOS(env));
    Rf_defineVar(nano_ResultSymbol, VECTOR_ELT(out, 1), ENCLOS(env));
    out = VECTOR_ELT(out, 1);
  } else {
    Rf_defineVar(nano_ResultSymbol, out, ENCLOS(env));
  }
  Rf_defineVar(nano_AioSymbol, R_NilValue, env);

  UNPROTECT(1);
  return out;

}

SEXP rnng_aio_get_msgdata2(SEXP env) {

  const SEXP exist = Rf_findVarInFrame(ENCLOS(env), nano_ResultSymbol);
  if (exist != R_UnboundValue)
    return exist;

  const SEXP aio = Rf_findVarInFrame(env, nano_AioSymbol);
  if (R_ExternalPtrTag(aio) != nano_AioSymbol)
    Rf_error("object is not a valid or active Aio");

  nano_aio *raio = (nano_aio *) R_ExternalPtrAddr(aio);

  int res;
  nano_cv_aio *ncva = (nano_cv_aio *) R_ExternalPtrAddr(Rf_getAttrib(aio, nano_CvSymbol));
  nng_mtx *mtx = ncva->cv->mtx;
  nng_mtx_lock(mtx);
  res = raio->result;
  nng_mtx_unlock(mtx);

  if (res == 0)
    return nano_unresolved;

  if (res > 0)
    return mk_error_raio(res, env);

  SEXP out;
  const int kpr = raio->mode > 0 ? 0 : 1, mod = kpr ? -raio->mode : raio->mode;
  unsigned char *buf;
  size_t sz;

  if (raio->type == IOV_RECVAIO) {
    buf = raio->data;
    sz = nng_aio_count(raio->aio);
  } else {
    nng_msg *msg = (nng_msg *) raio->data;
    buf = nng_msg_body(msg);
    sz = nng_msg_len(msg);
  }

  PROTECT(out = nano_decode(buf, sz, mod, kpr));
  if (kpr) {
    Rf_defineVar(nano_RawSymbol, VECTOR_ELT(out, 0), ENCLOS(env));
    Rf_defineVar(nano_ResultSymbol, VECTOR_ELT(out, 1), ENCLOS(env));
    out = VECTOR_ELT(out, 1);
  } else {
    Rf_defineVar(nano_ResultSymbol, out, ENCLOS(env));
  }
  Rf_defineVar(nano_AioSymbol, R_NilValue, env);

  UNPROTECT(1);
  return out;

}

SEXP rnng_aio_call(SEXP aio) {

  if (TYPEOF(aio) != ENVSXP)
    return aio;

  const SEXP coreaio = Rf_findVarInFrame(aio, nano_AioSymbol);
  if (R_ExternalPtrTag(coreaio) != nano_AioSymbol || R_ExternalPtrAddr(coreaio) == NULL)
    return aio;

  nano_aio *aiop = (nano_aio *) R_ExternalPtrAddr(coreaio);
  nng_aio_wait(aiop->aio);
  switch (aiop->type) {
  case RECVAIO:
  case IOV_RECVAIO:
  case HTTP_AIO:
    Rf_findVarInFrame(aio, nano_DataSymbol);
    break;
  case SENDAIO:
  case IOV_SENDAIO:
    Rf_findVarInFrame(aio, nano_ResultSymbol);
    break;
  }

  return aio;

}

SEXP rnng_aio_stop(SEXP aio) {

  if (TYPEOF(aio) != ENVSXP)
    return R_NilValue;

  SEXP coreaio = Rf_findVarInFrame(aio, nano_AioSymbol);
  if (R_ExternalPtrTag(coreaio) != nano_AioSymbol)
    return R_NilValue;

  nano_aio *aiop = (nano_aio *) R_ExternalPtrAddr(coreaio);
  nng_aio_stop(aiop->aio);
  Rf_defineVar(nano_AioSymbol, R_NilValue, aio);

  return R_NilValue;

}

SEXP rnng_unresolved(SEXP x) {

  if ((Rf_inherits(x, "recvAio") && Rf_inherits(Rf_findVarInFrame(x, nano_DataSymbol), "unresolvedValue")) ||
      (Rf_inherits(x, "sendAio") && Rf_inherits(Rf_findVarInFrame(x, nano_ResultSymbol), "unresolvedValue")) ||
      (Rf_inherits(x, "unresolvedValue")))
    return Rf_ScalarLogical(1);

  return Rf_ScalarLogical(0);

}

SEXP rnng_unresolved2(SEXP aio) {

  if (TYPEOF(aio) != ENVSXP)
    return Rf_ScalarLogical(0);

  SEXP coreaio = Rf_findVarInFrame(aio, nano_AioSymbol);
  if (R_ExternalPtrTag(coreaio) != nano_AioSymbol || R_ExternalPtrAddr(coreaio) == NULL)
    return Rf_ScalarLogical(0);

  nano_aio *aiop = (nano_aio *) R_ExternalPtrAddr(coreaio);

#if NNG_MAJOR_VERSION == 1 && NNG_MINOR_VERSION < 6
  int res;
  nng_mtx_lock(shr_mtx);
  res = aiop->result;
  nng_mtx_unlock(shr_mtx);
  return Rf_ScalarLogical(!res);
#else
  return Rf_ScalarLogical(nng_aio_busy(aiop->aio));
#endif

}

// send recv aio functions -----------------------------------------------------

SEXP rnng_send_aio(SEXP con, SEXP data, SEXP mode, SEXP timeout, SEXP clo) {

  const nng_duration dur = timeout == R_NilValue ? NNG_DURATION_DEFAULT : (nng_duration) Rf_asInteger(timeout);
  nano_aio *saio = R_Calloc(1, nano_aio);
  SEXP enc, aio;
  R_xlen_t xlen;
  unsigned char *dp;
  int xc;

  const SEXP ptrtag = R_ExternalPtrTag(con);
  if (ptrtag == nano_SocketSymbol) {

    nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(con);
    nng_msg *msg;

    enc = nano_encodes(data, mode);
    xlen = Rf_xlength(enc);
    dp = RAW(enc);

    saio->type = SENDAIO;

    if ((xc = nng_msg_alloc(&msg, 0))) {
      R_Free(saio);
      return mk_error_data(-xc);
    }
    if ((xc = nng_msg_append(msg, dp, xlen)) ||
        (xc = nng_aio_alloc(&saio->aio, saio_complete, saio))) {
      nng_msg_free(msg);
      R_Free(saio);
      return mk_error_data(-xc);
    }

    nng_aio_set_msg(saio->aio, msg);
    nng_aio_set_timeout(saio->aio, dur);
    nng_send_aio(*sock, saio->aio);

    PROTECT(aio = R_MakeExternalPtr(saio, nano_AioSymbol, R_NilValue));
    R_RegisterCFinalizerEx(aio, saio_finalizer, TRUE);

  } else if (ptrtag == nano_ContextSymbol) {

    nng_ctx *ctxp = (nng_ctx *) R_ExternalPtrAddr(con);
    nng_msg *msg;

    enc = nano_encodes(data, mode);
    xlen = Rf_xlength(enc);
    dp = RAW(enc);

    saio->type = SENDAIO;

    if ((xc = nng_msg_alloc(&msg, 0))) {
      R_Free(saio);
      return mk_error_data(-xc);
    }

    if ((xc = nng_msg_append(msg, dp, xlen)) ||
        (xc = nng_aio_alloc(&saio->aio, saio_complete, saio))) {
      nng_msg_free(msg);
      R_Free(saio);
      return mk_error_data(-xc);
    }

    nng_aio_set_msg(saio->aio, msg);
    nng_aio_set_timeout(saio->aio, dur);
    nng_ctx_send(*ctxp, saio->aio);

    PROTECT(aio = R_MakeExternalPtr(saio, nano_AioSymbol, R_NilValue));
    R_RegisterCFinalizerEx(aio, saio_finalizer, TRUE);

  } else if (ptrtag == nano_StreamSymbol) {

    nng_stream *sp = (nng_stream *) R_ExternalPtrAddr(con);
    const int frames = LOGICAL(Rf_getAttrib(con, nano_TextframesSymbol))[0];
    nng_iov iov;
    enc = nano_encode(data);
    xlen = Rf_xlength(enc);

    saio->type = IOV_SENDAIO;
    saio->data = R_Calloc(xlen, unsigned char);
    memcpy(saio->data, RAW(enc), xlen);
    iov.iov_len = frames == 1 ? xlen - 1 : xlen;
    iov.iov_buf = saio->data;

    if ((xc = nng_aio_alloc(&saio->aio, isaio_complete, saio))) {
      R_Free(saio->data);
      R_Free(saio);
      return mk_error_data(-xc);
    }

    if ((xc = nng_aio_set_iov(saio->aio, 1u, &iov))) {
      nng_aio_free(saio->aio);
      R_Free(saio->data);
      R_Free(saio);
      return mk_error_data(-xc);
    }

    nng_aio_set_timeout(saio->aio, dur);
    nng_stream_send(sp, saio->aio);

    PROTECT(aio = R_MakeExternalPtr(saio, nano_AioSymbol, R_NilValue));
    R_RegisterCFinalizerEx(aio, iaio_finalizer, TRUE);

  } else {
    error_return("'con' is not a valid Socket, Context or Stream");
  }

  SEXP env, fun;
  PROTECT(env = Rf_allocSExp(ENVSXP));
  SET_ENCLOS(env, clo);
  SET_ATTRIB(env, nano_sendAio);
  SET_OBJECT(env, 1);
  Rf_defineVar(nano_AioSymbol, aio, env);
  PROTECT(fun = Rf_allocSExp(CLOSXP));
  SET_FORMALS(fun, nano_aioFormals);
  SET_BODY(fun, CAR(nano_aioFuncs));
  SET_CLOENV(fun, clo);
  R_MakeActiveBinding(nano_ResultSymbol, fun, env);

  UNPROTECT(3);
  return env;

}

SEXP rnng_recv_aio(SEXP con, SEXP mode, SEXP timeout, SEXP keep, SEXP bytes, SEXP clo) {

  const int kpr = LOGICAL(keep)[0];
  const nng_duration dur = timeout == R_NilValue ? NNG_DURATION_DEFAULT : (nng_duration) Rf_asInteger(timeout);
  nano_aio *raio = R_Calloc(1, nano_aio);
  SEXP aio;
  int xc;

  const SEXP ptrtag = R_ExternalPtrTag(con);
  if (ptrtag == nano_SocketSymbol) {

    nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(con);
    raio->type = RECVAIO;
    raio->mode = kpr ? -nano_matcharg(mode) : nano_matcharg(mode);

    if ((xc = nng_aio_alloc(&raio->aio, raio_complete, raio))) {
      R_Free(raio);
      return kpr ? mk_error_recv(xc) : mk_error_data(xc);
    }

    nng_aio_set_timeout(raio->aio, dur);
    nng_recv_aio(*sock, raio->aio);

    PROTECT(aio = R_MakeExternalPtr(raio, nano_AioSymbol, R_NilValue));
    R_RegisterCFinalizerEx(aio, raio_finalizer, TRUE);

  } else if (ptrtag == nano_ContextSymbol) {

    nng_ctx *ctxp = (nng_ctx *) R_ExternalPtrAddr(con);
    raio->type = RECVAIO;
    raio->mode = kpr ? -nano_matcharg(mode) : nano_matcharg(mode);

    if ((xc = nng_aio_alloc(&raio->aio, raio_complete, raio))) {
      R_Free(raio);
      return kpr ? mk_error_recv(xc) : mk_error_data(xc);
    }

    nng_aio_set_timeout(raio->aio, dur);
    nng_ctx_recv(*ctxp, raio->aio);

    PROTECT(aio = R_MakeExternalPtr(raio, nano_AioSymbol, R_NilValue));
    R_RegisterCFinalizerEx(aio, raio_finalizer, TRUE);

  } else if (ptrtag == nano_StreamSymbol) {

    nng_stream *sp = (nng_stream *) R_ExternalPtrAddr(con);
    const size_t xlen = (size_t) Rf_asInteger(bytes);
    nng_iov iov;

    raio->type = IOV_RECVAIO;
    raio->mode = kpr ? -nano_matchargs(mode) : nano_matchargs(mode);
    raio->data = R_Calloc(xlen, unsigned char);
    iov.iov_len = xlen;
    iov.iov_buf = raio->data;

    if ((xc = nng_aio_alloc(&raio->aio, iraio_complete, raio))) {
      R_Free(raio->data);
      R_Free(raio);
      return kpr ? mk_error_recv(xc) : mk_error_data(xc);
    }

    if ((xc = nng_aio_set_iov(raio->aio, 1u, &iov))) {
      nng_aio_free(raio->aio);
      R_Free(raio->data);
      R_Free(raio);
      return kpr ? mk_error_recv(xc) : mk_error_data(xc);
    }

    nng_aio_set_timeout(raio->aio, dur);
    nng_stream_recv(sp, raio->aio);

    PROTECT(aio = R_MakeExternalPtr(raio, nano_AioSymbol, R_NilValue));
    R_RegisterCFinalizerEx(aio, iaio_finalizer, TRUE);

  } else {
    error_return("'con' is not a valid Socket, Context or Stream");
  }

  SEXP env, fun;
  PROTECT(env = Rf_allocSExp(ENVSXP));
  SET_ENCLOS(env, clo);
  SET_ATTRIB(env, nano_recvAio);
  SET_OBJECT(env, 1);
  Rf_defineVar(nano_AioSymbol, aio, env);

  if (kpr) {
    PROTECT(fun = Rf_allocSExp(CLOSXP));
    SET_FORMALS(fun, nano_aioFormals);
    SET_BODY(fun, CADDR(nano_aioFuncs));
    SET_CLOENV(fun, clo);
    R_MakeActiveBinding(nano_RawSymbol, fun, env);
    UNPROTECT(1);
  }
  PROTECT(fun = Rf_allocSExp(CLOSXP));
  SET_FORMALS(fun, nano_aioFormals);
  SET_BODY(fun, CADR(nano_aioFuncs));
  SET_CLOENV(fun, clo);
  R_MakeActiveBinding(nano_DataSymbol, fun, env);

  UNPROTECT(3);
  return env;

}

// ncurl aio -------------------------------------------------------------------

SEXP rnng_ncurl_aio(SEXP http, SEXP convert, SEXP method, SEXP headers, SEXP data,
                    SEXP timeout, SEXP tls, SEXP clo) {

  const char *httr = CHAR(STRING_ELT(http, 0));
  nano_aio *haio = R_Calloc(1, nano_aio);
  nano_handle *handle = R_Calloc(1, nano_handle);
  int xc;
  SEXP aio;

  haio->type = HTTP_AIO;
  haio->data = handle;
  haio->mode = LOGICAL(convert)[0];
  handle->cfg = NULL;

  if ((xc = nng_url_parse(&handle->url, httr)))
    goto exitlevel1;
  if ((xc = nng_http_client_alloc(&handle->cli, handle->url)))
    goto exitlevel2;
  if ((xc = nng_http_req_alloc(&handle->req, handle->url)))
    goto exitlevel3;

  if (method != R_NilValue) {
    const char *met = CHAR(STRING_ELT(method, 0));
    if ((xc = nng_http_req_set_method(handle->req, met)))
      goto exitlevel4;
  }

  if (headers != R_NilValue) {
    R_xlen_t hlen = Rf_xlength(headers);
    SEXP names = Rf_getAttrib(headers, R_NamesSymbol);
    switch (TYPEOF(headers)) {
    case STRSXP:
      for (R_xlen_t i = 0; i < hlen; i++) {
        const char *head = CHAR(STRING_ELT(headers, i));
        const char *name = CHAR(STRING_ELT(names, i));
        if ((xc = nng_http_req_set_header(handle->req, name, head)))
          goto exitlevel4;
      }
      break;
    case VECSXP:
      for (R_xlen_t i = 0; i < hlen; i++) {
        const char *head = CHAR(STRING_ELT(VECTOR_ELT(headers, i), 0));
        const char *name = CHAR(STRING_ELT(names, i));
        if ((xc = nng_http_req_set_header(handle->req, name, head)))
          goto exitlevel4;
      }
      break;
    }
  }

  if (data != R_NilValue) {
    SEXP enc = nano_encode(data);
    unsigned char *dp = RAW(enc);
    const size_t dlen = Rf_xlength(enc) - 1;
    if ((xc = nng_http_req_set_data(handle->req, dp, dlen)))
      goto exitlevel4;
  }

  if ((xc = nng_http_res_alloc(&handle->res)))
    goto exitlevel4;

  if ((xc = nng_aio_alloc(&haio->aio, iraio_complete, haio)))
    goto exitlevel5;

  if (!strcmp(handle->url->u_scheme, "https")) {

    if (tls == R_NilValue) {
      if ((xc = nng_tls_config_alloc(&handle->cfg, NNG_TLS_MODE_CLIENT)))
        goto exitlevel6;

      if ((xc = nng_tls_config_server_name(handle->cfg, handle->url->u_hostname)) ||
          (xc = nng_tls_config_auth_mode(handle->cfg, NNG_TLS_AUTH_MODE_NONE)) ||
          (xc = nng_http_client_set_tls(handle->cli, handle->cfg)))
        goto exitlevel7;

    } else {

      if (R_ExternalPtrTag(tls) != nano_TlsSymbol)
        Rf_error("'tls' is not a valid TLS Configuration");
      handle->cfg = (nng_tls_config *) R_ExternalPtrAddr(tls);
      nng_tls_config_hold(handle->cfg);

      if ((xc = nng_tls_config_server_name(handle->cfg, handle->url->u_hostname)) ||
          (xc = nng_http_client_set_tls(handle->cli, handle->cfg)))
        goto exitlevel7;
    }

  }

  if (timeout != R_NilValue)
    nng_aio_set_timeout(haio->aio, (nng_duration) Rf_asInteger(timeout));
  nng_http_client_transact(handle->cli, handle->req, handle->res, haio->aio);

  PROTECT(aio = R_MakeExternalPtr(haio, nano_AioSymbol, R_NilValue));
  R_RegisterCFinalizerEx(aio, haio_finalizer, TRUE);

  SEXP env, fun;
  PROTECT(env = Rf_allocSExp(ENVSXP));
  SET_ENCLOS(env, clo);
  SET_ATTRIB(env, nano_ncurlAio);
  SET_OBJECT(env, 1);
  Rf_defineVar(nano_AioSymbol, aio, env);
  int i = 0;
  for (SEXP fnlist = nano_aioNFuncs; fnlist != R_NilValue; fnlist = CDR(fnlist)) {
    PROTECT(fun = Rf_allocSExp(CLOSXP));
    SET_FORMALS(fun, nano_aioFormals);
    SET_BODY(fun, CAR(fnlist));
    SET_CLOENV(fun, clo);
    switch (++i) {
    case 1: R_MakeActiveBinding(nano_StatusSymbol, fun, env);
    case 2: R_MakeActiveBinding(nano_HeadersSymbol, fun, env);
    case 3: R_MakeActiveBinding(nano_RawSymbol, fun, env);
    case 4: R_MakeActiveBinding(nano_DataSymbol, fun, env);
    }
    UNPROTECT(1);
  }

  UNPROTECT(2);
  return env;

  exitlevel7:
  nng_tls_config_free(handle->cfg);
  exitlevel6:
  nng_aio_free(haio->aio);
  exitlevel5:
  nng_http_res_free(handle->res);
  exitlevel4:
  nng_http_req_free(handle->req);
  exitlevel3:
  nng_http_client_free(handle->cli);
  exitlevel2:
  nng_url_free(handle->url);
  exitlevel1:
  R_Free(handle);
  R_Free(haio);
  return mk_error_ncurl(xc);

}

SEXP rnng_aio_http(SEXP env, SEXP response, SEXP type) {

  const int typ = INTEGER(type)[0];
  SEXP exist;
  switch (typ) {
  case 1: exist = Rf_findVarInFrame(ENCLOS(env), nano_StatusSymbol); break;
  case 2: exist = Rf_findVarInFrame(ENCLOS(env), nano_StateSymbol); break;
  case 3: exist = Rf_findVarInFrame(ENCLOS(env), nano_RawSymbol); break;
  default: exist = Rf_findVarInFrame(ENCLOS(env), nano_ResultSymbol); break;
  }
  if (exist != R_UnboundValue)
    return exist;

  const SEXP aio = Rf_findVarInFrame(env, nano_AioSymbol);
  if (R_ExternalPtrTag(aio) != nano_AioSymbol)
    Rf_error("object is not a valid or active Aio");

  nano_aio *haio = (nano_aio *) R_ExternalPtrAddr(aio);

#if NNG_MAJOR_VERSION == 1 && NNG_MINOR_VERSION < 6
  int res;
  nng_mtx_lock(shr_mtx);
  res = haio->result;
  nng_mtx_unlock(shr_mtx);
  if (res == 0)
#else
  if (nng_aio_busy(haio->aio))
#endif
    return nano_unresolved;

  if (haio->result > 0)
    return mk_error_haio(haio->result, env);

  void *dat;
  size_t sz;
  SEXP out, vec, cvec, rvec;
  nano_handle *handle = (nano_handle *) haio->data;

  uint16_t code = nng_http_res_get_status(handle->res), relo = code >= 300 && code < 400 ? 1 : 0;
  Rf_defineVar(nano_StatusSymbol, Rf_ScalarInteger(code), ENCLOS(env));

  if (relo) {
    const R_xlen_t rlen = Rf_xlength(response);
    switch (TYPEOF(response)) {
    case STRSXP:
      PROTECT(response = Rf_lengthgets(response, rlen + 1));
      SET_STRING_ELT(response, rlen, Rf_mkChar("Location"));
      break;
    case VECSXP:
      PROTECT(response = Rf_lengthgets(response, rlen + 1));
      SET_VECTOR_ELT(response, rlen, Rf_mkString("Location"));
      break;
    default:
      PROTECT(response = Rf_mkString("Location"));
    }
  }

  if (response != R_NilValue) {
    const R_xlen_t rlen = Rf_xlength(response);
    PROTECT(rvec = Rf_allocVector(VECSXP, rlen));

    switch (TYPEOF(response)) {
    case STRSXP:
      for (R_xlen_t i = 0; i < rlen; i++) {
        const char *r = nng_http_res_get_header(handle->res, CHAR(STRING_ELT(response, i)));
        SET_VECTOR_ELT(rvec, i, r == NULL ? R_NilValue : Rf_mkString(r));
      }
      Rf_namesgets(rvec, response);
      break;
    case VECSXP: ;
      SEXP rnames;
      PROTECT(rnames = Rf_allocVector(STRSXP, rlen));
      for (R_xlen_t i = 0; i < rlen; i++) {
        SEXP rname = STRING_ELT(VECTOR_ELT(response, i), 0);
        SET_STRING_ELT(rnames, i, rname);
        const char *r = nng_http_res_get_header(handle->res, CHAR(rname));
        SET_VECTOR_ELT(rvec, i, r == NULL ? R_NilValue : Rf_mkString(r));
      }
      Rf_namesgets(rvec, rnames);
      UNPROTECT(1);
      break;
    }
    UNPROTECT(1);
  } else {
    rvec = R_NilValue;
  }
  Rf_defineVar(nano_StateSymbol, rvec, ENCLOS(env));
  if (relo) UNPROTECT(1);

  nng_http_res_get_data(handle->res, &dat, &sz);
  vec = Rf_allocVector(RAWSXP, sz);
  if (dat != NULL)
    memcpy(RAW(vec), dat, sz);
  Rf_defineVar(nano_RawSymbol, vec, ENCLOS(env));

  if (haio->mode) {
    int xc;
    PROTECT(cvec = Rf_lang2(nano_RtcSymbol, vec));
    cvec = R_tryEvalSilent(cvec, R_BaseEnv, &xc);
    UNPROTECT(1);
  } else {
    cvec = R_NilValue;
  }
  Rf_defineVar(nano_ResultSymbol, cvec, ENCLOS(env));
  Rf_defineVar(nano_AioSymbol, R_NilValue, env);

  switch (typ) {
  case 1: out = Rf_findVarInFrame(ENCLOS(env), nano_StatusSymbol); break;
  case 2: out = Rf_findVarInFrame(ENCLOS(env), nano_StateSymbol); break;
  case 3: out = Rf_findVarInFrame(ENCLOS(env), nano_RawSymbol); break;
  default: out = Rf_findVarInFrame(ENCLOS(env), nano_ResultSymbol); break;
  }
  return out;

}

// ncurl session ---------------------------------------------------------------

SEXP rnng_ncurl_session(SEXP http, SEXP convert, SEXP method, SEXP headers, SEXP data,
                        SEXP response, SEXP timeout, SEXP tls) {

  const char *httr = CHAR(STRING_ELT(http, 0));
  nano_aio *haio = R_Calloc(1, nano_aio);
  nano_handle *handle = R_Calloc(1, nano_handle);

  int xc;
  SEXP sess, aio;

  haio->type = HTTP_AIO;
  haio->data = handle;
  haio->mode = LOGICAL(convert)[0];
  handle->cfg = NULL;

  if ((xc = nng_url_parse(&handle->url, httr)))
    goto exitlevel1;
  if ((xc = nng_http_client_alloc(&handle->cli, handle->url)))
    goto exitlevel2;
  if ((xc = nng_http_req_alloc(&handle->req, handle->url)))
    goto exitlevel3;

  if (method != R_NilValue) {
    const char *met = CHAR(STRING_ELT(method, 0));
    if ((xc = nng_http_req_set_method(handle->req, met)))
      goto exitlevel4;
  }

  if (headers != R_NilValue) {
    R_xlen_t hlen = Rf_xlength(headers);
    SEXP names = Rf_getAttrib(headers, R_NamesSymbol);
    switch (TYPEOF(headers)) {
    case STRSXP:
      for (R_xlen_t i = 0; i < hlen; i++) {
        const char *head = CHAR(STRING_ELT(headers, i));
        const char *name = CHAR(STRING_ELT(names, i));
        if ((xc = nng_http_req_set_header(handle->req, name, head)))
          goto exitlevel4;
      }
      break;
    case VECSXP:
      for (R_xlen_t i = 0; i < hlen; i++) {
        const char *head = CHAR(STRING_ELT(VECTOR_ELT(headers, i), 0));
        const char *name = CHAR(STRING_ELT(names, i));
        if ((xc = nng_http_req_set_header(handle->req, name, head)))
          goto exitlevel4;
      }
      break;
    }
  }

  if (data != R_NilValue) {
    SEXP enc = nano_encode(data);
    unsigned char *dp = RAW(enc);
    const size_t dlen = Rf_xlength(enc) - 1;
    if ((xc = nng_http_req_set_data(handle->req, dp, dlen)))
      goto exitlevel4;
  }

  if ((xc = nng_http_res_alloc(&handle->res)))
    goto exitlevel4;

  if ((xc = nng_aio_alloc(&haio->aio, iraio_complete, haio)))
    goto exitlevel5;

  if (!strcmp(handle->url->u_scheme, "https")) {

    if (tls == R_NilValue) {
      if ((xc = nng_tls_config_alloc(&handle->cfg, NNG_TLS_MODE_CLIENT)))
        goto exitlevel6;

      if ((xc = nng_tls_config_server_name(handle->cfg, handle->url->u_hostname)) ||
          (xc = nng_tls_config_auth_mode(handle->cfg, NNG_TLS_AUTH_MODE_NONE)) ||
          (xc = nng_http_client_set_tls(handle->cli, handle->cfg)))
        goto exitlevel7;

    } else {

      if (R_ExternalPtrTag(tls) != nano_TlsSymbol)
        Rf_error("'tls' is not a valid TLS Configuration");
      handle->cfg = (nng_tls_config *) R_ExternalPtrAddr(tls);
      nng_tls_config_hold(handle->cfg);

      if ((xc = nng_tls_config_server_name(handle->cfg, handle->url->u_hostname)) ||
          (xc = nng_http_client_set_tls(handle->cli, handle->cfg)))
        goto exitlevel7;
    }

  }

  if (timeout != R_NilValue)
    nng_aio_set_timeout(haio->aio, (nng_duration) Rf_asInteger(timeout));
  nng_http_client_connect(handle->cli, haio->aio);
  nng_aio_wait(haio->aio);
  if ((xc = haio->result) > 0)
    goto exitlevel7;

  nng_http_conn *conn;
  conn = nng_aio_get_output(haio->aio, 0);

  PROTECT(sess = R_MakeExternalPtr(conn, nano_SessionSymbol, R_NilValue));
  R_RegisterCFinalizerEx(sess, session_finalizer, TRUE);
  SET_ATTRIB(sess, nano_ncurlSession);
  SET_OBJECT(sess, 1);

  PROTECT(aio = R_MakeExternalPtr(haio, nano_AioSymbol, R_NilValue));
  R_RegisterCFinalizerEx(aio, haio_finalizer, TRUE);
  Rf_setAttrib(sess, nano_AioSymbol, aio);

  if (response != R_NilValue)
    Rf_setAttrib(sess, nano_ResponseSymbol, response);

  UNPROTECT(2);
  return sess;

  exitlevel7:
    if (handle->cfg != NULL)
      nng_tls_config_free(handle->cfg);
  exitlevel6:
    nng_aio_free(haio->aio);
  exitlevel5:
    nng_http_res_free(handle->res);
  exitlevel4:
    nng_http_req_free(handle->req);
  exitlevel3:
    nng_http_client_free(handle->cli);
  exitlevel2:
    nng_url_free(handle->url);
  exitlevel1:
    R_Free(handle);
  R_Free(haio);
  ERROR_RET(xc);

}

SEXP rnng_ncurl_transact(SEXP session) {

  if (R_ExternalPtrTag(session) != nano_SessionSymbol)
    Rf_error("'session' is not a valid or active ncurlSession");

  nng_http_conn *conn = (nng_http_conn *) R_ExternalPtrAddr(session);
  SEXP aio = Rf_getAttrib(session, nano_AioSymbol);
  nano_aio *haio = (nano_aio *) R_ExternalPtrAddr(aio);
  nano_handle *handle = (nano_handle *) haio->data;

  nng_http_conn_transact(conn, handle->req, handle->res, haio->aio);
  nng_aio_wait(haio->aio);
  if (haio->result > 0)
    return mk_error_ncurl(haio->result);

  SEXP out, vec, rvec, cvec, response;
  void *dat;
  size_t sz;
  const char *names[] = {"status", "headers", "raw", "data", ""};

  PROTECT(out = Rf_mkNamed(VECSXP, names));

  uint16_t code = nng_http_res_get_status(handle->res);
  SET_VECTOR_ELT(out, 0, Rf_ScalarInteger(code));

  response = Rf_getAttrib(session, nano_ResponseSymbol);
  if (response != R_NilValue) {
    const R_xlen_t rlen = Rf_xlength(response);
    PROTECT(rvec = Rf_allocVector(VECSXP, rlen));

    switch (TYPEOF(response)) {
    case STRSXP:
      for (R_xlen_t i = 0; i < rlen; i++) {
        const char *r = nng_http_res_get_header(handle->res, CHAR(STRING_ELT(response, i)));
        SET_VECTOR_ELT(rvec, i, r == NULL ? R_NilValue : Rf_mkString(r));
      }
      Rf_namesgets(rvec, response);
      break;
    case VECSXP: ;
      SEXP rnames;
      PROTECT(rnames = Rf_allocVector(STRSXP, rlen));
      for (R_xlen_t i = 0; i < rlen; i++) {
        SEXP rname = STRING_ELT(VECTOR_ELT(response, i), 0);
        SET_STRING_ELT(rnames, i, rname);
        const char *r = nng_http_res_get_header(handle->res, CHAR(rname));
        SET_VECTOR_ELT(rvec, i, r == NULL ? R_NilValue : Rf_mkString(r));
      }
      Rf_namesgets(rvec, rnames);
      UNPROTECT(1);
      break;
    }
    UNPROTECT(1);
  } else {
    rvec = R_NilValue;
  }
  SET_VECTOR_ELT(out, 1, rvec);

  nng_http_res_get_data(handle->res, &dat, &sz);
  vec = Rf_allocVector(RAWSXP, sz);
  if (dat != NULL)
    memcpy(RAW(vec), dat, sz);
  SET_VECTOR_ELT(out, 2, vec);

  if (haio->mode) {
    int xc;
    PROTECT(cvec = Rf_lang2(nano_RtcSymbol, vec));
    cvec = R_tryEvalSilent(cvec, R_BaseEnv, &xc);
    UNPROTECT(1);
  } else {
    cvec = R_NilValue;
  }
  SET_VECTOR_ELT(out, 3, cvec);

  UNPROTECT(1);
  return out;

}

SEXP rnng_ncurl_session_close(SEXP session) {

  if (R_ExternalPtrTag(session) != nano_SessionSymbol)
    Rf_error("'session' is not a valid or active ncurlSession");

  nng_http_conn *sp = (nng_http_conn *) R_ExternalPtrAddr(session);
  nng_http_conn_close(sp);
  R_SetExternalPtrTag(session, R_NilValue);
  R_ClearExternalPtr(session);
  Rf_setAttrib(session, nano_AioSymbol, R_NilValue);
  Rf_setAttrib(session, nano_ResponseSymbol, R_NilValue);

  return nano_success;

}

// request ---------------------------------------------------------------------

SEXP rnng_request(SEXP con, SEXP data, SEXP sendmode, SEXP recvmode, SEXP timeout, SEXP keep, SEXP clo) {

  if (R_ExternalPtrTag(con) != nano_ContextSymbol)
    Rf_error("'context' is not a valid Context");

  nng_ctx *ctx = (nng_ctx *) R_ExternalPtrAddr(con);

  int xc;
  const int kpr = LOGICAL(keep)[0];
  const nng_duration dur = timeout == R_NilValue ? NNG_DURATION_DEFAULT : (nng_duration) Rf_asInteger(timeout);

  SEXP enc, sendaio, aio, env, fun;
  R_xlen_t xlen;
  unsigned char *dp;
  nng_msg *msg;

  enc = nano_encodes(data, sendmode);
  xlen = Rf_xlength(enc);
  dp = RAW(enc);

  nano_aio *saio = R_Calloc(1, nano_aio);

  if ((xc = nng_msg_alloc(&msg, 0)))
    return kpr ? mk_error_recv(xc) : mk_error_data(xc);

  if ((xc = nng_msg_append(msg, dp, xlen)) ||
      (xc = nng_aio_alloc(&saio->aio, saio_complete, saio))) {
    nng_msg_free(msg);
    return kpr ? mk_error_recv(xc) : mk_error_data(xc);
  }

  nng_aio_set_msg(saio->aio, msg);
  nng_ctx_send(*ctx, saio->aio);

  nano_aio *raio = R_Calloc(1, nano_aio);

  raio->type = RECVAIO;
  raio->mode = kpr ? -nano_matcharg(recvmode) : nano_matcharg(recvmode);

  if ((xc = nng_aio_alloc(&raio->aio, raio_complete, raio))) {
    R_Free(raio);
    nng_aio_free(saio->aio);
    R_Free(saio);
    return kpr ? mk_error_recv(xc) : mk_error_data(xc);
  }

  nng_aio_set_timeout(raio->aio, dur);
  nng_ctx_recv(*ctx, raio->aio);

  PROTECT(aio = R_MakeExternalPtr(raio, nano_AioSymbol, R_NilValue));
  R_RegisterCFinalizerEx(aio, raio_finalizer, TRUE);

  PROTECT(env = Rf_allocSExp(ENVSXP));
  SET_ENCLOS(env, clo);
  SET_ATTRIB(env, nano_recvAio);
  SET_OBJECT(env, 1);
  Rf_defineVar(nano_AioSymbol, aio, env);
  PROTECT(sendaio = R_MakeExternalPtr(saio, R_NilValue, R_NilValue));
  Rf_setAttrib(sendaio, nano_ContextSymbol, con);
  R_RegisterCFinalizerEx(sendaio, reqsaio_finalizer, TRUE);
  Rf_defineVar(nano_ContextSymbol, sendaio, ENCLOS(env));

  if (kpr) {
    PROTECT(fun = Rf_allocSExp(CLOSXP));
    SET_FORMALS(fun, nano_aioFormals);
    SET_BODY(fun, CADDR(nano_aioFuncs));
    SET_CLOENV(fun, clo);
    R_MakeActiveBinding(nano_RawSymbol, fun, env);
    UNPROTECT(1);
  }
  PROTECT(fun = Rf_allocSExp(CLOSXP));
  SET_FORMALS(fun, nano_aioFormals);
  SET_BODY(fun, CADR(nano_aioFuncs));
  SET_CLOENV(fun, clo);
  R_MakeActiveBinding(nano_DataSymbol, fun, env);

  UNPROTECT(4);
  return env;

}

// cv specials -----------------------------------------------------------------

SEXP rnng_cv_alloc(void) {

  nano_cv *cvp = R_Calloc(1, nano_cv);
  SEXP xp;
  int xc;

  xc = nng_mtx_alloc(&cvp->mtx);
  if (xc)
    ERROR_OUT(xc);

  xc = nng_cv_alloc(&cvp->cv, cvp->mtx);
  if (xc) {
    nng_mtx_free(cvp->mtx);
    ERROR_OUT(xc);
  }

  PROTECT(xp = R_MakeExternalPtr(cvp, nano_CvSymbol, R_NilValue));
  R_RegisterCFinalizerEx(xp, cv_finalizer, TRUE);
  Rf_classgets(xp, Rf_mkString("conditionVariable"));

  UNPROTECT(1);
  return xp;

}

SEXP rnng_cv_wait(SEXP cvar) {

  if (R_ExternalPtrTag(cvar) != nano_CvSymbol)
    Rf_error("'cv' is not a valid Condition Variable");

  nano_cv *ncv = (nano_cv *) R_ExternalPtrAddr(cvar);
  nng_cv *cv = ncv->cv;
  nng_mtx *mtx = ncv->mtx;

  nng_mtx_lock(mtx);
  while (ncv->condition == 0)
    nng_cv_wait(cv);
  ncv->condition--;
  nng_mtx_unlock(mtx);

  return ncv->flag ? Rf_ScalarLogical(0) : Rf_ScalarLogical(1);

}

SEXP rnng_cv_until(SEXP cvar, SEXP msec) {

  if (R_ExternalPtrTag(cvar) != nano_CvSymbol)
    Rf_error("'cv' is not a valid Condition Variable");

  nano_cv *ncv = (nano_cv *) R_ExternalPtrAddr(cvar);
  nng_cv *cv = ncv->cv;
  nng_mtx *mtx = ncv->mtx;

  nng_time time = nng_clock();
  switch (TYPEOF(msec)) {
  case INTSXP:
    time = time + (nng_time) INTEGER(msec)[0];
    break;
  case REALSXP:
    time = time + (nng_time) Rf_asInteger(msec);
    break;
  }

  uint8_t signalled = 1;
  nng_mtx_lock(mtx);
  while (ncv->condition == 0) {
    if (nng_cv_until(cv, time) == NNG_ETIMEDOUT) {
      signalled = 0;
      break;
    }
  }
  if (signalled) ncv->condition--;
  nng_mtx_unlock(mtx);

  return ncv->flag ? Rf_ScalarLogical(0) : Rf_ScalarLogical(1);

}

SEXP rnng_cv_reset(SEXP cvar) {

  if (R_ExternalPtrTag(cvar) != nano_CvSymbol)
    Rf_error("'cv' is not a valid Condition Variable");

  nano_cv *ncv = (nano_cv *) R_ExternalPtrAddr(cvar);
  nng_mtx *mtx = ncv->mtx;

  nng_mtx_lock(mtx);
  ncv->flag = 0;
  ncv->condition = 0;
  nng_mtx_unlock(mtx);

  return R_NilValue;

}

SEXP rnng_cv_value(SEXP cvar) {

  if (R_ExternalPtrTag(cvar) != nano_CvSymbol)
    Rf_error("'cv' is not a valid Condition Variable");
  nano_cv *ncv = (nano_cv *) R_ExternalPtrAddr(cvar);
  nng_mtx *mtx = ncv->mtx;
  int cond;
  nng_mtx_lock(mtx);
  cond = ncv->condition;
  nng_mtx_unlock(mtx);

  return Rf_ScalarInteger(cond);

}

SEXP rnng_cv_signal(SEXP cvar) {

  if (R_ExternalPtrTag(cvar) != nano_CvSymbol)
    Rf_error("'cv' is not a valid Condition Variable");

  nano_cv *ncv = (nano_cv *) R_ExternalPtrAddr(cvar);
  nng_cv *cv = ncv->cv;
  nng_mtx *mtx = ncv->mtx;

  nng_mtx_lock(mtx);
  ncv->condition++;
  nng_cv_wake(cv);
  nng_mtx_unlock(mtx);

  return R_NilValue;

}

SEXP rnng_cv_recv_aio(SEXP con, SEXP mode, SEXP timeout, SEXP keep, SEXP bytes, SEXP clo, SEXP cvar) {

  if (R_ExternalPtrTag(cvar) != nano_CvSymbol)
    Rf_error("'cv' is not a valid Condition Variable");

  nano_cv *cvp = (nano_cv *) R_ExternalPtrAddr(cvar);
  const int kpr = LOGICAL(keep)[0];
  const nng_duration dur = timeout == R_NilValue ? NNG_DURATION_DEFAULT : (nng_duration) Rf_asInteger(timeout);

  nano_cv_aio *cv_raio = R_Calloc(1, nano_cv_aio);
  nano_aio *raio = R_Calloc(1, nano_aio);
  SEXP aio;
  int xc;

  cv_raio->aio = raio;
  cv_raio->cv = cvp;

  const SEXP ptrtag = R_ExternalPtrTag(con);
  if (ptrtag == nano_SocketSymbol) {

    nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(con);
    raio->type = RECVAIO;
    raio->mode = kpr ? -nano_matcharg(mode) : nano_matcharg(mode);

    if ((xc = nng_aio_alloc(&raio->aio, raio_complete_signal, cv_raio))) {
      R_Free(cv_raio);
      R_Free(raio);
      return kpr ? mk_error_recv(xc) : mk_error_data(xc);
    }

    nng_aio_set_timeout(raio->aio, dur);
    nng_recv_aio(*sock, raio->aio);

    PROTECT(aio = R_MakeExternalPtr(raio, nano_AioSymbol, R_NilValue));
    R_RegisterCFinalizerEx(aio, raio_finalizer, TRUE);
    UNPROTECT(1);

  } else if (ptrtag == nano_ContextSymbol) {

    nng_ctx *ctxp = (nng_ctx *) R_ExternalPtrAddr(con);
    raio->type = RECVAIO;
    raio->mode = kpr ? -nano_matcharg(mode) : nano_matcharg(mode);

    if ((xc = nng_aio_alloc(&raio->aio, raio_complete_signal, cv_raio))) {
      R_Free(cv_raio);
      R_Free(raio);
      return kpr ? mk_error_recv(xc) : mk_error_data(xc);
    }

    nng_aio_set_timeout(raio->aio, dur);
    nng_ctx_recv(*ctxp, raio->aio);

    PROTECT(aio = R_MakeExternalPtr(raio, nano_AioSymbol, R_NilValue));
    R_RegisterCFinalizerEx(aio, raio_finalizer, TRUE);
    UNPROTECT(1);

  } else if (ptrtag == nano_StreamSymbol) {

    nng_stream *sp = (nng_stream *) R_ExternalPtrAddr(con);
    const size_t xlen = (size_t) Rf_asInteger(bytes);
    nng_iov iov;

    raio->type = IOV_RECVAIO;
    raio->mode = kpr ? -nano_matchargs(mode) : nano_matchargs(mode);
    raio->data = R_Calloc(xlen, unsigned char);
    iov.iov_len = xlen;
    iov.iov_buf = raio->data;

    if ((xc = nng_aio_alloc(&raio->aio, iraio_complete_signal, cv_raio))) {
      R_Free(cv_raio);
      R_Free(raio->data);
      R_Free(raio);
      return kpr ? mk_error_recv(xc) : mk_error_data(xc);
    }

    if ((xc = nng_aio_set_iov(raio->aio, 1u, &iov))) {
      nng_aio_free(raio->aio);
      R_Free(cv_raio);
      R_Free(raio->data);
      R_Free(raio);
      return kpr ? mk_error_recv(xc) : mk_error_data(xc);
    }

    nng_aio_set_timeout(raio->aio, dur);
    nng_stream_recv(sp, raio->aio);

    PROTECT(aio = R_MakeExternalPtr(raio, nano_AioSymbol, R_NilValue));
    R_RegisterCFinalizerEx(aio, iaio_finalizer, TRUE);
    UNPROTECT(1);

  } else {
    Rf_error("'con' is not a valid Socket, Context or Stream");
  }

  PROTECT(aio);
  SEXP env, fun, signal;
  PROTECT(env = Rf_allocSExp(ENVSXP));
  SET_ENCLOS(env, clo);
  SET_ATTRIB(env, nano_recvAio);
  SET_OBJECT(env, 1);
  Rf_defineVar(nano_AioSymbol, aio, env);
  PROTECT(signal = R_MakeExternalPtr(cv_raio, R_NilValue, R_NilValue));
  R_RegisterCFinalizerEx(signal, cv_aio_finalizer, TRUE);
  Rf_setAttrib(aio, nano_CvSymbol, signal);

  if (kpr) {
    PROTECT(fun = Rf_allocSExp(CLOSXP));
    SET_FORMALS(fun, nano_aioFormals);
    SET_BODY(fun, CAD4R(nano_aioFuncs));
    SET_CLOENV(fun, clo);
    R_MakeActiveBinding(nano_RawSymbol, fun, env);
    UNPROTECT(1);
  }
  PROTECT(fun = Rf_allocSExp(CLOSXP));
  SET_FORMALS(fun, nano_aioFormals);
  SET_BODY(fun, CADDDR(nano_aioFuncs));
  SET_CLOENV(fun, clo);
  R_MakeActiveBinding(nano_DataSymbol, fun, env);

  UNPROTECT(4);
  return env;

}

SEXP rnng_cv_request(SEXP con, SEXP data, SEXP sendmode, SEXP recvmode, SEXP timeout, SEXP keep, SEXP clo, SEXP cvar) {

  if (R_ExternalPtrTag(con) != nano_ContextSymbol)
    Rf_error("'context' is not a valid Context");
  if (R_ExternalPtrTag(cvar) != nano_CvSymbol)
    Rf_error("'cv' is not a valid Condition Variable");

  nng_ctx *ctx = (nng_ctx *) R_ExternalPtrAddr(con);
  nano_cv *cvp = (nano_cv *) R_ExternalPtrAddr(cvar);

  int xc;
  const int kpr = LOGICAL(keep)[0];
  const nng_duration dur = timeout == R_NilValue ? NNG_DURATION_DEFAULT : (nng_duration) Rf_asInteger(timeout);

  SEXP enc, sendaio, aio, env, fun, signal;
  R_xlen_t xlen;
  unsigned char *dp;
  nng_msg *msg;

  enc = nano_encodes(data, sendmode);
  xlen = Rf_xlength(enc);
  dp = RAW(enc);

  nano_aio *saio = R_Calloc(1, nano_aio);

  if ((xc = nng_msg_alloc(&msg, 0)))
    return kpr ? mk_error_recv(xc) : mk_error_data(xc);

  if ((xc = nng_msg_append(msg, dp, xlen)) ||
      (xc = nng_aio_alloc(&saio->aio, saio_complete, saio))) {
    nng_msg_free(msg);
    return kpr ? mk_error_recv(xc) : mk_error_data(xc);
  }

  nng_aio_set_msg(saio->aio, msg);
  nng_ctx_send(*ctx, saio->aio);

  nano_aio *raio = R_Calloc(1, nano_aio);
  raio->type = RECVAIO;
  raio->mode = kpr ? -nano_matcharg(recvmode) : nano_matcharg(recvmode);

  nano_cv_aio *cv_raio = R_Calloc(1, nano_cv_aio);
  cv_raio->aio = raio;
  cv_raio->cv = cvp;

  if ((xc = nng_aio_alloc(&raio->aio, raio_complete_signal, cv_raio))) {
    R_Free(cv_raio);
    R_Free(raio);
    nng_aio_free(saio->aio);
    R_Free(saio);
    return kpr ? mk_error_recv(xc) : mk_error_data(xc);
  }

  nng_aio_set_timeout(raio->aio, dur);
  nng_ctx_recv(*ctx, raio->aio);

  PROTECT(aio = R_MakeExternalPtr(raio, nano_AioSymbol, R_NilValue));
  R_RegisterCFinalizerEx(aio, raio_finalizer, TRUE);

  PROTECT(env = Rf_allocSExp(ENVSXP));
  SET_ENCLOS(env, clo);
  SET_ATTRIB(env, nano_recvAio);
  SET_OBJECT(env, 1);
  Rf_defineVar(nano_AioSymbol, aio, env);
  PROTECT(signal = R_MakeExternalPtr(cv_raio, R_NilValue, R_NilValue));
  R_RegisterCFinalizerEx(signal, cv_aio_finalizer, TRUE);
  Rf_setAttrib(aio, nano_CvSymbol, signal);
  PROTECT(sendaio = R_MakeExternalPtr(saio, R_NilValue, R_NilValue));
  Rf_setAttrib(sendaio, nano_ContextSymbol, con);
  R_RegisterCFinalizerEx(sendaio, reqsaio_finalizer, TRUE);
  Rf_defineVar(nano_ContextSymbol, sendaio, ENCLOS(env));

  if (kpr) {
    PROTECT(fun = Rf_allocSExp(CLOSXP));
    SET_FORMALS(fun, nano_aioFormals);
    SET_BODY(fun, CAD4R(nano_aioFuncs));
    SET_CLOENV(fun, clo);
    R_MakeActiveBinding(nano_RawSymbol, fun, env);
    UNPROTECT(1);
  }
  PROTECT(fun = Rf_allocSExp(CLOSXP));
  SET_FORMALS(fun, nano_aioFormals);
  SET_BODY(fun, CADDDR(nano_aioFuncs));
  SET_CLOENV(fun, clo);
  R_MakeActiveBinding(nano_DataSymbol, fun, env);

  UNPROTECT(5);
  return env;

}

// pipes -----------------------------------------------------------------------

SEXP rnng_pipe_notify(SEXP socket, SEXP cv, SEXP cv2, SEXP add, SEXP remove, SEXP flag) {

  if (R_ExternalPtrTag(socket) != nano_SocketSymbol)
    Rf_error("'socket' is not a valid Socket");

  if (R_ExternalPtrTag(cv) != nano_CvSymbol)
    Rf_error("'cv' is not a valid Condition Variable");

  nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(socket);
  nano_cv *cvp = (nano_cv *) R_ExternalPtrAddr(cv);
  int xc;

  if (cv2 != R_NilValue) {

    if (R_ExternalPtrTag(cv2) != nano_CvSymbol)
      Rf_error("'cv2' is not a valid Condition Variable");
    nano_cv *cvp2 = (nano_cv *) R_ExternalPtrAddr(cv2);
    nano_cv_duo *duo = R_Calloc(1, nano_cv_duo);
    duo->cv = cvp;
    duo->cv2 = cvp2;

    if (LOGICAL(add)[0]) {
      xc = nng_pipe_notify(*sock, NNG_PIPE_EV_ADD_POST,
                           LOGICAL(flag)[0] ? pipe_cb_flag_cv_duo : pipe_cb_signal_cv_duo, duo);
      if (xc)
        ERROR_OUT(xc);
    }
    if (LOGICAL(remove)[0]) {
      xc = nng_pipe_notify(*sock, NNG_PIPE_EV_REM_POST,
                           LOGICAL(flag)[0] ? pipe_cb_flag_cv_duo : pipe_cb_signal_cv_duo, duo);
      if (xc)
        ERROR_OUT(xc);
    }

    SEXP duoptr;
    PROTECT(duoptr = R_MakeExternalPtr(duo, R_NilValue, R_NilValue));
    R_RegisterCFinalizerEx(duoptr, cv_duo_finalizer, TRUE);
    R_MakeWeakRef(cv, duoptr, R_NilValue, FALSE);
    UNPROTECT(1);

  } else {

    if (LOGICAL(add)[0]) {
      xc = nng_pipe_notify(*sock, NNG_PIPE_EV_ADD_POST,
                           LOGICAL(flag)[0] ? pipe_cb_flag_cv : pipe_cb_signal_cv, cvp);
      if (xc)
        ERROR_OUT(xc);
    }
    if (LOGICAL(remove)[0]) {
      xc = nng_pipe_notify(*sock, NNG_PIPE_EV_REM_POST,
                           LOGICAL(flag)[0] ? pipe_cb_flag_cv : pipe_cb_signal_cv, cvp);
      if (xc)
        ERROR_OUT(xc);
    }

  }

  return nano_success;

}

SEXP rnng_socket_lock(SEXP socket, SEXP cv) {

  if (R_ExternalPtrTag(socket) != nano_SocketSymbol)
    Rf_error("'socket' is not a valid Socket");
  nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(socket);

  int xc;
  if (cv != R_NilValue) {
    if (R_ExternalPtrTag(cv) != nano_CvSymbol)
      Rf_error("'cv' is not a valid Condition Variable");
    nano_cv *ncv = (nano_cv *) R_ExternalPtrAddr(cv);
    xc = nng_pipe_notify(*sock, NNG_PIPE_EV_ADD_PRE, pipe_cb_dropcon, ncv);
  } else {
    xc = nng_pipe_notify(*sock, NNG_PIPE_EV_ADD_PRE, pipe_cb_dropcon, NULL);
  }

  if (xc)
    ERROR_OUT(xc);

  return nano_success;

}

SEXP rnng_socket_unlock(SEXP socket) {

  if (R_ExternalPtrTag(socket) != nano_SocketSymbol)
    Rf_error("'socket' is not a valid Socket");

  nng_socket *sock = (nng_socket *) R_ExternalPtrAddr(socket);
  int xc;
  xc = nng_pipe_notify(*sock, NNG_PIPE_EV_ADD_PRE, NULL, NULL);
  if (xc)
    ERROR_OUT(xc);

  return nano_success;

}
