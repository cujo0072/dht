#ifndef _DHASH_H_
#define _DHASH_H_
/*
 *
 * Copyright (C) 2001  Frank Dabek (fdabek@lcs.mit.edu), 
 *                     Frans Kaashoek (kaashoek@lcs.mit.edu),
 *   		       Massachusetts Institute of Technology
 * 
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <arpc.h>
#include <async.h>
#include <dhash_prot.h>
#include <chord_prot.h>
#include <dbfe.h>
#include <callback.h>
#include <refcnt.h>
#include <chord.h>
#include <qhash.h>
#include <sys/time.h>
#include <chord.h>
/*
 *
 * dhash.h
 *
 * Include file for the distributed hash service
 */

#include "vsc.h"

struct store_cbstate;

typedef callback<void, ptr<dbrec>, dhash_stat>::ptr cbvalue;
typedef callback<void, struct store_cbstate *,dhash_stat>::ptr cbstat;
typedef callback<void,dhash_stat>::ptr cbstore;
typedef callback<void,dhash_stat>::ptr cbstat_t;

#define MTU 1024

struct store_cbstate {
  svccb *sbp;
  int nreplica;
  int r;
  dhash_insertarg *item;
  cbstat cb;
  store_cbstate (svccb *sbpi, int ni, dhash_insertarg *ii, cbstat cbi) :
    sbp (sbpi), nreplica (ni), item (ii), cb (cbi) 
  { r = nreplica + 1; };
};

struct store_state {
  chordID key;
  unsigned int read;
  unsigned int size;
  char *buf;
  route path;

  ihash_entry <store_state> link;

  store_state (chordID k, int z) : key(k), 
    read(0), size(z), buf(New char[z]) { };

  ~store_state () { 
    delete[] buf; 
  };
};

struct query_succ_state {
  vec<chord_node> succ;
  int pathlen;
  svccb *sbp;
  ptr<dhash_fetch_arg> rarg;
  chordID source;

  query_succ_state (vec<chord_node> s, int p, svccb *sb,
		    ptr<dhash_fetch_arg> r, chordID so) :
    succ (s), pathlen (p), sbp (sb), rarg (r), source (so) {};
};
class dhashclient {

  ptr<axprt_stream> x;
  ptr<asrv> clntsrv;
  ptr<chord> clntnode;

  int do_caching;

  ihash<chordID, store_state, &store_state::key, &store_state::link, hashID> pst;

  void doRPC (chordID ID, rpc_program prog, int procno,
	      ptr<void> in, void *out, aclnt_cb cb);

  bool straddled (route path, chordID &k);
  void dispatch (svccb *sbp);

  void lookup_res_cb (svccb *sbp, dhash_fetchrecurs_res *res, clnt_stat err);
  void lookup_iter_cb (svccb *sbp, 
		       dhash_fetchiter_res *res,
		       route path,
		       int nerror,
		       clnt_stat err);

  void query_successors (vec<chord_node> succ, 
			 int pathlen,
			 svccb *sbp,
			 ptr<dhash_fetch_arg> rarg,
			 chordID source);
  
  void query_successors_fetch_cb (query_succ_state *st,
				  chordID prev,
				  dhash_fetchiter_res *fres, 
				  clnt_stat err);

  void insert_findsucc_cb (svccb *sbp, ptr<dhash_insertarg> item, chordID succ,
			   route path, chordstat err);
  void insert_store_cb(svccb *sbp,  dhash_storeres *res,
		       ptr<dhash_insertarg> item,
		       chordID source,
		       clnt_stat err);

  void transfer_cb (chordID key, svccb *sbp, dhash_fetchiter_res *res, clnt_stat err);
  void send_cb (svccb *sbp, dhash_storeres *res, 
		      chordID source, clnt_stat err);

  void cache_on_path (chordID key, route path);
  void send_block (chordID key, chordID to, store_status stat);
  void send_store_cb (dhash_storeres *res, clnt_stat err);

  void memorize_block (chordID key, dhash_fetchiter_res *res, route path);
  void memorize_block (chordID key, dhash_fetchiter_res *res);
  void memorize_block (chordID key, int tsize, 
		       int offset, void *base, int dsize);
  bool block_memorized (chordID key);
  void forget_block (chordID key);

 public:  
  void set_caching(char c) { do_caching = c;};
  dhashclient (ptr<axprt_stream> x, ptr<chord> clnt);
};

class dhash {

  int nreplica;
  int kc_delay;
  int rc_delay;
  int ss_mode;

  dbfe *db;
  vnode *host_node;
  net_address t_source;

  ihash<chordID, store_state, &store_state::key, &store_state::link, hashID> pst;

  int qnonce;
  qhash<int, unsigned long> rqc;

  void doRPC (chordID ID, rpc_program prog, int procno,
	      ptr<void> in, void *out, aclnt_cb cb);

  void dhash_reply (long xid, unsigned long procno, void *res);

  void dispatch (unsigned long, chord_RPC_arg *, unsigned long);
  void fetchsvc_cb (long xid, dhash_fetch_arg *arg, ptr<dbrec> val, dhash_stat err);
  void storesvc_cb (long xid, dhash_insertarg *arg, dhash_stat err);
  
  void fetch_cb (cbvalue cb,  ptr<dbrec> ret);

  void fetchiter_svc_cb (long xid, dhash_fetch_arg *farg,
			 ptr<dbrec> val, dhash_stat stat);

  void fetchrecurs_havedata_cb (unsigned long rpc_id,
				dhash_recurs_arg *rarg,
				unsigned int nonce,
				chord_node return_address,
				ptr<dbrec> val, dhash_stat err);
  void fetchrecurs_sent_data (dhash_stat *done_res, clnt_stat err);
  void fetchrecurs_continue (dhash_fetchrecurs_res *res, clnt_stat err);

  void store (dhash_insertarg *arg, cbstore cb);
  void store_cb(store_status type, chordID id, cbstore cb, int stat);
  void store_repl_cb (cbstore cb, dhash_stat err);
  bool store_complete (dhash_insertarg *arg);

  void get_keys_traverse_cb (ptr<vec<chordID> > vKeys,
			     chordID mypred,
			     chordID predid,
			     chordID key);

  void init_key_status ();
  void transfer_initial_keys ();
  void transfer_init_getkeys_cb (dhash_getkeys_res *res, clnt_stat err);
  void transfer_init_gotk_cb (dhash_stat err);

  void update_replica_list ();
  bool isReplica(chordID id);
  void replicate_key (chordID key, cbstat_t cb);
  void replicate_key_cb (unsigned int replicas_done, cbstat_t cb, chordID key,
			 dhash_stat err);

  void install_keycheck_timer ();
  void check_keys_timer_cb ();
  void check_keys_traverse_cb (chordID key);

  void install_replica_timer ();
  void check_replicas_cb ();
  void check_replicas ();
  void check_replicas_traverse_cb (chordID to, chordID key);
  void fix_replicas_txerd (dhash_stat err);

  void change_status (chordID key, dhash_stat newstatus);

  void transfer_key (chordID to, chordID key, store_status stat, 
		     callback<void, dhash_stat>::ref cb);
  void transfer_fetch_cb (chordID to, chordID key, store_status stat, 
			  callback<void, dhash_stat>::ref cb,
			  ptr<dbrec> data, dhash_stat err);
  void transfer_store_cb (callback<void, dhash_stat>::ref cb, 
			  dhash_storeres *res, ptr<dhash_insertarg> i_arg,
			  chordID to, clnt_stat err);

  void get_key (chordID source, chordID key, cbstat_t cb);
  void get_key_initread_cb (cbstat_t cb, dhash_fetchiter_res *res, 
			    chordID source, 
			    chordID key, clnt_stat err);
  void get_key_read_cb (chordID key, char *buf, unsigned int *read, 
			dhash_fetchiter_res *res, cbstat_t cb, clnt_stat err);
  void get_key_finish (char *buf, unsigned int size, chordID key, cbstat_t cb);
  void get_key_finish_store (cbstat_t cb, int err);

  void store_flush (chordID key, dhash_stat value);
  void store_flush_cb (int err);
  void cache_flush (chordID key, dhash_stat value);
  void cache_flush_cb (int err);

  void transfer_key_cb (chordID key, dhash_stat err);

  char responsible(chordID& n);

  void printkeys ();
  void printkeys_walk (chordID k);
  void printcached_walk (chordID k);


  ptr<dbrec> id2dbrec(chordID id);
  chordID dbrec2id (ptr<dbrec> r);

  vs_cache<chordID, dhash_stat> key_store;
  vs_cache<chordID, dhash_stat> key_replicate;
  vs_cache<chordID, dhash_stat> key_cache;
  
  chordID pred;
  vec<chordID> replicas;
  timecb_t *check_replica_tcb;
  timecb_t *check_key_tcb;

  /* statistics */
  long bytes_stored;
  long keys_stored;
  long keys_replicated;
  long keys_cached;
  long bytes_served;

 public:
  dhash (str dbname, vnode *node, 
	 int nreplica = 0, int ss = 10000, int cs = 1000, int ss_mode = 0);
  void accept(ptr<axprt_stream> x);

  void print_stats ();
  void stop ();
  void fetch (chordID id, cbvalue cb);
  dhash_stat key_status(chordID n);
    
};


#endif
