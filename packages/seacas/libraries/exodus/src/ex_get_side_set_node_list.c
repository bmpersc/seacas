/*
 * Copyright (c) 2005 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government
 * retains certain rights in this software.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *     * Neither the name of Sandia Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*****************************************************************************
*
* exgssn - ex_get_side_set_node_list
*
* entry conditions -
*   input parameters:
*       int     exoid                   exodus file id
*       int     side_set_id             side set id
*
* exit conditions -
*       int     *side_set_node_cnt_list returned array of number of nodes for
*                                       side or face
*       int     *side_set_node_list     array of nodes
*
* revision history -
*
*
*****************************************************************************/

#include "exodusII.h"     // for ex_err, exerrval, ex_block, etc
#include "exodusII_int.h" // for elem_blk_parm, EX_FATAL, etc
#include <ctype.h>        // for toupper
#include <inttypes.h>     // for PRId64
#include <stddef.h>       // for size_t
#include <stdio.h>
#include <stdlib.h>    // for malloc, NULL, free
#include <string.h>    // for strncmp, strlen
#include <sys/types.h> // for int64_t
/*!
 * This routine is designed to read the Exodus II V 2.0 side set side
 * definition  and return a ExodusI style side set node definition.
 */
static void set_count(int exoid, void_int *cnt, size_t ndx, size_t val)
{
  if (ex_int64_status(exoid) & EX_BULK_INT64_API) {
    ((int64_t *)cnt)[ndx] = val;
  }
  else {
    ((int *)cnt)[ndx] = val;
  }
}

static int check_valid_side(size_t side_num, size_t max_sides, char *topology, int exoid)
{
  char errmsg[MAX_ERR_LENGTH];
  int  err_stat = EX_NOERR;

  exerrval = 0;
  if (side_num + 1 < 1 || side_num + 1 > max_sides) {
    exerrval = EX_BADPARAM;
    snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: Invalid %s edge number %" ST_ZU " in file id %d",
             topology, side_num + 1, exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);
    err_stat = EX_FATAL;
  }
  return err_stat;
}

static void get_nodes(int exoid, void_int *to, size_t ito, void_int *from, size_t ifrom)
{
  if (ex_int64_status(exoid) & EX_BULK_INT64_API) {
    ((int64_t *)to)[ito] = ((int64_t *)from)[ifrom];
  }
  else {
    ((int *)to)[ito] = ((int *)from)[ifrom];
  }
}

int ex_get_side_set_node_list(int exoid, ex_entity_id side_set_id, void_int *side_set_node_cnt_list,
                              void_int *side_set_node_list)
{
  size_t    i, j;
  int64_t   elem, side;
  int64_t   num_side_sets, num_elem_blks, num_df, ndim;
  int64_t   tot_num_elem = 0, tot_num_ss_elem = 0, elem_num = 0;
  size_t    connect_offset, side_num, node_pos;
  void_int *elem_blk_ids       = NULL;
  void_int *connect            = NULL;
  void_int *ss_elem_ndx        = NULL;
  void_int *ss_elem_node_ndx   = NULL;
  void_int *ss_parm_ndx        = NULL;
  void_int *side_set_elem_list = NULL;
  void_int *side_set_side_list = NULL;
  size_t    elem_ctr, node_ctr, elem_num_pos;
  size_t    num_nodes_per_elem;
  int       int_size, ids_size;

  int err_stat = EX_NOERR;
  int status;

  struct elem_blk_parm *elem_blk_parms = NULL;

  /* side to node translation tables -
     These tables are used to look up the side number based on the
     first and second node in the side/face list. The side node order
     is found in the original Exodus document, SAND87-2997. The element
     node order is found in the ExodusII document, SAND92-2137. These
     tables were generated by following the right-hand rule for determining
     the outward normal.
  */
  /* triangle */
  static int tri_table[3][3] = {
      {1, 2, 4}, /* side 1 */
      {2, 3, 5}, /* side 2 */
      {3, 1, 6}  /* side 3 */
  };

  /* triangle 3d */
  static int tri3_table[5][7] = {
      {1, 2, 3, 4, 5, 6, 7}, /* side 1 (face) */
      {3, 2, 1, 6, 5, 4, 7}, /* side 2 (face) */
      {1, 2, 4, 0, 0, 0, 0}, /* side 3 (edge) */
      {2, 3, 5, 0, 0, 0, 0}, /* side 4 (edge) */
      {3, 1, 6, 0, 0, 0, 0}  /* side 5 (edge) */
  };

  /* quad */
  static int quad_table[4][3] = {
      {1, 2, 5}, /* side 1 */
      {2, 3, 6}, /* side 2 */
      {3, 4, 7}, /* side 3 */
      {4, 1, 8}  /* side 4 */
  };

  /* shell */
  static int shell_table[6][9] = {
      {1, 2, 3, 4, 5, 6, 7, 8, 9}, /* side 1 (face) */
      {1, 4, 3, 2, 8, 7, 6, 5, 9}, /* side 2 (face) */
      {1, 2, 5, 0, 0, 0, 0, 0, 0}, /* side 3 (edge) */
      {2, 3, 6, 0, 0, 0, 0, 0, 0}, /* side 4 (edge) */
      {3, 4, 7, 0, 0, 0, 0, 0, 0}, /* side 5 (edge) */
      {4, 1, 8, 0, 0, 0, 0, 0, 0}  /* side 6 (edge) */
  };

  /* tetra */
  static int tetra_table[4][7] = {
      {1, 2, 4, 5, 9, 8, 14},  /* Side 1 nodes */
      {2, 3, 4, 6, 10, 9, 12}, /* Side 2 nodes */
      {1, 4, 3, 8, 10, 7, 13}, /* Side 3 nodes */
      {1, 3, 2, 7, 6, 5, 11}   /* Side 4 nodes */
  };

  /* wedge */
  static int wedge_table[5][9] = {
      {1, 2, 5, 4, 7, 11, 13, 10, 20}, /* Side 1 nodes -- quad     */
      {2, 3, 6, 5, 8, 12, 14, 11, 18}, /* Side 2 nodes -- quad     */
      {1, 4, 6, 3, 10, 15, 12, 9, 19}, /* Side 3 nodes -- quad     */
      {1, 3, 2, 0, 9, 8, 7, 0, 16},    /* Side 4 nodes -- triangle */
      {4, 5, 6, 0, 13, 14, 15, 0, 17}  /* Side 5 nodes -- triangle */
  };

  static int wedge18_table[5][9] = {
      {1, 2, 5, 4, 7, 11, 13, 10, 16}, /* Side 1 nodes -- quad     */
      {2, 3, 6, 5, 8, 12, 14, 11, 17}, /* Side 2 nodes -- quad     */
      {1, 4, 6, 3, 10, 15, 12, 9, 18}, /* Side 3 nodes -- quad     */
      {1, 3, 2, 0, 9, 8, 7, 0, 0},     /* Side 4 nodes -- triangle */
      {4, 5, 6, 0, 13, 14, 15, 0, 0}   /* Side 5 nodes -- triangle */
  };

  /* hex */
  static int hex_table[6][9] = {
      {1, 2, 6, 5, 9, 14, 17, 13, 26},  /* side 1 */
      {2, 3, 7, 6, 10, 15, 18, 14, 25}, /* side 2 */
      {3, 4, 8, 7, 11, 16, 19, 15, 27}, /* side 3 */
      {1, 5, 8, 4, 13, 20, 16, 12, 24}, /* side 4 */
      {1, 4, 3, 2, 12, 11, 10, 9, 22},  /* side 5 */
      {5, 6, 7, 8, 17, 18, 19, 20, 23}  /* side 6 */
  };

  /* pyramid */
  static int pyramid_table[5][8] = {
      {1, 2, 5, 0, 6, 11, 10, 0}, /* side 1 (tri) */
      {2, 3, 5, 0, 7, 12, 11, 0}, /* side 2 (tri) */
      {3, 4, 5, 0, 8, 13, 12, 0}, /* side 3 (tri) */
      {1, 5, 4, 0, 10, 13, 9, 0}, /* side 4 (tri) */
      {1, 4, 3, 2, 9, 8, 7, 6}    /* side 5 (quad) */
  };

  char errmsg[MAX_ERR_LENGTH];

  exerrval = 0; /* clear error code */

  /* first check if any side sets are specified */
  /* inquire how many side sets have been stored */
  num_side_sets = ex_inquire_int(exoid, EX_INQ_SIDE_SETS);
  if (num_side_sets < 0) {
    snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: failed to get number of side sets in file id %d",
             exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);
    return (EX_FATAL);
  }

  if (num_side_sets == 0) {
    snprintf(errmsg, MAX_ERR_LENGTH, "Warning: no side sets defined in file id %d", exoid);
    ex_err("ex_get_side_set_node_list", errmsg, EX_WARN);
    return (EX_WARN);
  }

  /* Lookup index of side set id in VAR_SS_IDS array */
  ex_id_lkup(exoid, EX_SIDE_SET, side_set_id);
  if (exerrval != 0) {
    if (exerrval == EX_NULLENTITY) {
      snprintf(errmsg, MAX_ERR_LENGTH, "Warning: side set %" PRId64 " is NULL in file id %d",
               side_set_id, exoid);
      ex_err("ex_get_side_set_node_list", errmsg, EX_NULLENTITY);
      return (EX_WARN);
    }

    snprintf(errmsg, MAX_ERR_LENGTH,
             "ERROR: failed to locate side set %" PRId64 " in VAR_SS_IDS array in file id %d",
             side_set_id, exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);
    return (EX_FATAL);
  }

  num_elem_blks = ex_inquire_int(exoid, EX_INQ_ELEM_BLK);
  if (num_elem_blks < 0) {
    snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: failed to get number of element blocks in file id %d",
             exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);
    return (EX_FATAL);
  }

  tot_num_elem = ex_inquire_int(exoid, EX_INQ_ELEM);
  if (tot_num_elem < 0) {
    snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: failed to get total number of elements in file id %d",
             exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);
    return (EX_FATAL);
  }

  /* get the dimensionality of the coordinates;  this is necessary to
     distinguish between 2d TRIs and 3d TRIs */
  ndim = ex_inquire_int(exoid, EX_INQ_DIM);
  if (ndim < 0) {
    snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: failed to get dimensionality in file id %d", exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);
    return (EX_FATAL);
  }

  int_size = sizeof(int);
  if (ex_int64_status(exoid) & EX_BULK_INT64_API) {
    int_size = sizeof(int64_t);
  }

  ids_size = sizeof(int);
  if (ex_int64_status(exoid) & EX_IDS_INT64_API) {
    ids_size = sizeof(int64_t);
  }

  /* First determine the  # of elements in the side set*/
  if (int_size == sizeof(int64_t)) {
    status = ex_get_set_param(exoid, EX_SIDE_SET, side_set_id, &tot_num_ss_elem, &num_df);
  }
  else {
    int tot, df;
    status          = ex_get_set_param(exoid, EX_SIDE_SET, side_set_id, &tot, &df);
    tot_num_ss_elem = tot;
    num_df          = df;
  }

  if (status != EX_NOERR) {
    snprintf(errmsg, MAX_ERR_LENGTH,
             "ERROR: failed to get number of elements in side set %" PRId64 " in file id %d",
             side_set_id, exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);
    return (EX_FATAL);
  }

  /* Allocate space for the side set element list */
  if (!(side_set_elem_list = malloc(tot_num_ss_elem * int_size))) {
    exerrval = EX_MEMFAIL;
    snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: failed to allocate space for side set element list "
                                     "for file id %d",
             exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);
    return (EX_FATAL);
  }

  /* Allocate space for the side set side list */
  if (!(side_set_side_list = malloc(tot_num_ss_elem * int_size))) {
    exerrval = EX_MEMFAIL;
    snprintf(errmsg, MAX_ERR_LENGTH,
             "ERROR: failed to allocate space for side set side list for file id %d", exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);
    err_stat = EX_FATAL;
    goto cleanup;
  }

  if (ex_get_set(exoid, EX_SIDE_SET, side_set_id, side_set_elem_list, side_set_side_list) == -1) {
    snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: failed to get side set %" PRId64 " in file id %d",
             side_set_id, exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);
    err_stat = EX_FATAL;
    goto cleanup;
  }

  /* Allocate space for the ss element index array */
  if (!(ss_elem_ndx = malloc(tot_num_ss_elem * int_size))) {
    exerrval = EX_MEMFAIL;
    snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: failed to allocate space for side set elem sort "
                                     "array for file id %d",
             exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);
    err_stat = EX_FATAL;
    goto cleanup;
  }

  /* Sort side set element list into index array  - non-destructive */
  if (int_size == sizeof(int64_t)) {
    /* Sort side set element list into index array  - non-destructive */
    int64_t *elems = (int64_t *)ss_elem_ndx;
    for (i = 0; i < tot_num_ss_elem; i++) {
      elems[i] = i; /* init index array to current position */
    }
    ex_iqsort64(side_set_elem_list, ss_elem_ndx, tot_num_ss_elem);
  }
  else {
    /* Sort side set element list into index array  - non-destructive */
    int *elems = (int *)ss_elem_ndx;
    for (i = 0; i < tot_num_ss_elem; i++) {
      elems[i] = i; /* init index array to current position */
    }
    ex_iqsort(side_set_elem_list, ss_elem_ndx, tot_num_ss_elem);
  }

  /* Allocate space for the element block ids */
  if (!(elem_blk_ids = malloc(num_elem_blks * ids_size))) {
    exerrval = EX_MEMFAIL;
    snprintf(errmsg, MAX_ERR_LENGTH,
             "ERROR: failed to allocate space for element block ids for file id %d", exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);
    err_stat = EX_FATAL;
    goto cleanup;
  }

  if (ex_get_elem_blk_ids(exoid, elem_blk_ids) == -1) {
    snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: failed to get element block ids in file id %d", exoid);
    ex_err("ex_get_side_set_node_list", errmsg, EX_MSG);
    err_stat = EX_FATAL;
    goto cleanup;
  }

  /* Allocate space for the element block params */
  if (!(elem_blk_parms = malloc(num_elem_blks * sizeof(struct elem_blk_parm)))) {
    exerrval = EX_MEMFAIL;
    snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: failed to allocate space for element block params "
                                     "for file id %d",
             exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);
    err_stat = EX_FATAL;
    goto cleanup;
  }

  elem_ctr = 0;
  for (i = 0; i < num_elem_blks; i++) {
    ex_entity_id id;
    if (ids_size == sizeof(int64_t)) {
      id = ((int64_t *)elem_blk_ids)[i];
    }
    else {
      id = ((int *)elem_blk_ids)[i];
    }

    err_stat = ex_int_get_block_param(exoid, id, ndim, &elem_blk_parms[i]);
    if (err_stat != EX_NOERR) {
      goto cleanup;
    }

    elem_ctr += elem_blk_parms[i].num_elem_in_blk;
    elem_blk_parms[i].elem_ctr = elem_ctr; /* save elem number max */
  }

  /* Allocate space for the ss element to element block parameter index array */
  if (!(ss_parm_ndx = malloc(tot_num_ss_elem * int_size))) {
    exerrval = EX_MEMFAIL;
    snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: failed to allocate space for side set elem parms "
                                     "index for file id %d",
             exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);
    err_stat = EX_FATAL;
    goto cleanup;
  }

  /* Allocate space for the ss element to node list index array */
  if (!(ss_elem_node_ndx = malloc(tot_num_ss_elem * int_size))) {
    exerrval = EX_MEMFAIL;
    snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: failed to allocate space for side set elem to node "
                                     "index for file id %d",
             exoid);
    ex_err("ex_get_side_set_node_list", errmsg, exerrval);

    err_stat = EX_FATAL;
    goto cleanup;
  }

  /* Build side set element to node list index and side set element
     parameter index.
  */
  node_ctr = 0;
  for (i = 0; i < tot_num_ss_elem; i++) {
    if (ex_int64_status(exoid) & EX_BULK_INT64_API) {
      elem = ((int64_t *)side_set_elem_list)[i];
      side = ((int64_t *)side_set_side_list)[i];
    }
    else {
      elem = ((int *)side_set_elem_list)[i];
      side = ((int *)side_set_side_list)[i];
    }

    for (j = 0; j < num_elem_blks; j++) {
      if (elem_blk_parms[j].elem_type_val != EX_EL_NULL_ELEMENT) {
        if (elem <= elem_blk_parms[j].elem_ctr) {
          break;
        }
      }
    }

    if (j >= num_elem_blks) {
      exerrval = EX_BADPARAM;
      snprintf(errmsg, MAX_ERR_LENGTH,
               "ERROR: Invalid element number %" PRId64 " found in side set %" PRId64 " in file %d",
               elem, side_set_id, exoid);
      ex_err("ex_get_side_set_node_list", errmsg, EX_MSG);
      err_stat = EX_FATAL;
      goto cleanup;
    }

    if (int_size == sizeof(int64_t)) {
      ((int64_t *)ss_parm_ndx)[i]      = j;        /* assign parameter block index */
      ((int64_t *)ss_elem_node_ndx)[i] = node_ctr; /* assign node list index */
    }
    else {
      ((int *)ss_parm_ndx)[i]      = j;        /* assign parameter block index */
      ((int *)ss_elem_node_ndx)[i] = node_ctr; /* assign node list index */
    }

    /* Update node_ctr (which points to next node in chain */
    node_ctr += elem_blk_parms[j].num_nodes_per_side[side - 1];
  }

  /* All setup, ready to go ... */

  elem_ctr = 0;

  for (j = 0; j < tot_num_ss_elem; j++) {
    int64_t elem_ndx;
    size_t  parm_ndx;
    if (ex_int64_status(exoid) & EX_BULK_INT64_API) {
      elem_ndx = ((int64_t *)ss_elem_ndx)[j];
      elem     = ((int64_t *)side_set_elem_list)[elem_ndx];
      side     = ((int64_t *)side_set_side_list)[elem_ndx];
      parm_ndx = ((int64_t *)ss_parm_ndx)[elem_ndx];
    }
    else {
      elem_ndx = ((int *)ss_elem_ndx)[j];
      elem     = ((int *)side_set_elem_list)[elem_ndx];
      side     = ((int *)side_set_side_list)[elem_ndx];
      parm_ndx = ((int *)ss_parm_ndx)[elem_ndx];
    }

    if (elem > elem_ctr) {
      /* release connectivity array space and get next one */
      if (elem_ctr > 0) {
        ex_safe_free(connect);
      }

      /* Allocate space for the connectivity array for new element block */
      if (!(connect = malloc(elem_blk_parms[parm_ndx].num_elem_in_blk *
                             elem_blk_parms[parm_ndx].num_nodes_per_elem * int_size))) {
        exerrval = EX_MEMFAIL;
        snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: failed to allocate space for connectivity "
                                         "array for file id %d",
                 exoid);
        ex_err("ex_get_side_set_node_list", errmsg, exerrval);
        err_stat = EX_FATAL;
        goto cleanup;
      }

      /* get connectivity array */
      if (ex_get_elem_conn(exoid, elem_blk_parms[parm_ndx].elem_blk_id, connect) == -1) {
        snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: failed to allocate space for connectivity "
                                         "array for file id %d",
                 exoid);
        ex_err("ex_get_side_set_node_list", errmsg, exerrval);
        err_stat = EX_FATAL;
        goto cleanup;
      }
      elem_ctr = elem_blk_parms[parm_ndx].elem_ctr;
    }

    if (connect == NULL) {
      snprintf(errmsg, MAX_ERR_LENGTH,
               "ERROR: internal error -- connect pointer is NULL for file id %d", exoid);
      ex_err("ex_get_side_set_node_list", errmsg, exerrval);
      err_stat = EX_FATAL;
      goto cleanup;
    }

    /*  For each side in side set, use the appropriate lookup table to
        determine the nodes from the connect array. */

    elem_num = elem - 1; /* element number 0-based*/
    /* calculate the relative element number position in it's block*/

    elem_num_pos =
        elem_num - (elem_blk_parms[parm_ndx].elem_ctr - elem_blk_parms[parm_ndx].num_elem_in_blk);

    /* calculate the beginning of the node list for this element by
       using the ss_elem_node_ndx index into the side_sets_node_index
       and adding the element number position * number of nodes per elem */

    num_nodes_per_elem = elem_blk_parms[parm_ndx].num_nodes_per_elem;
    connect_offset     = num_nodes_per_elem * elem_num_pos;
    side_num           = side - 1;

    if (int_size == sizeof(int64_t)) {
      node_pos = ((int64_t *)ss_elem_node_ndx)[elem_ndx];
    }
    else {
      node_pos = ((int *)ss_elem_node_ndx)[elem_ndx];
    }

    switch (elem_blk_parms[parm_ndx].elem_type_val) {
    case EX_EL_CIRCLE:
    case EX_EL_SPHERE: { /* Note: no side-node lookup table is used for this
                            simple case */
      get_nodes(exoid, side_set_node_list, node_pos, connect, connect_offset);
      set_count(exoid, side_set_node_cnt_list, elem_ndx, 1); /* 1 node object */
      break;
    }
    case EX_EL_TRUSS:
    case EX_EL_BEAM: { /* Note: no side-node lookup table is used for this
                          simple case */
      for (i = 0; i < num_nodes_per_elem; i++) {
        get_nodes(exoid, side_set_node_list, node_pos + i, connect, connect_offset + i);
      }
      set_count(exoid, side_set_node_cnt_list, elem_ndx, num_nodes_per_elem);
      break;
    }
    case EX_EL_TRIANGLE: {
      if (ndim == 2) { /* 2d TRIs */
        if (check_valid_side(side_num, 3, "triangle", exoid) != EX_NOERR) {
          goto cleanup;
        }

        get_nodes(exoid, side_set_node_list, node_pos, connect,
                  connect_offset + tri_table[side_num][0] - 1);
        get_nodes(exoid, side_set_node_list, node_pos + 1, connect,
                  connect_offset + tri_table[side_num][1] - 1);
        set_count(exoid, side_set_node_cnt_list, elem_ndx, 2); /* 2 node object */
        if (num_nodes_per_elem > 3)                            /* 6-node TRI  */
        {
          get_nodes(exoid, side_set_node_list, node_pos + 2, connect,
                    connect_offset + tri_table[side_num][2] - 1);
          set_count(exoid, side_set_node_cnt_list, elem_ndx, 3); /* 3 node object */
        }
      }
      else if (ndim == 3) { /* 3d TRIs */
        if (check_valid_side(side_num, 5, "triangle", exoid) != EX_NOERR) {
          goto cleanup;
        }

        get_nodes(exoid, side_set_node_list, node_pos, connect,
                  connect_offset + tri3_table[side_num][0] - 1);
        get_nodes(exoid, side_set_node_list, node_pos + 1, connect,
                  connect_offset + tri3_table[side_num][1] - 1);
        set_count(exoid, side_set_node_cnt_list, elem_ndx, 2); /* 2 node object */
        if (side_num + 1 <= 2)                                 /* 3, 4, 6, 7-node face */
        {
          if (num_nodes_per_elem == 3) /* 3-node face */
          {
            set_count(exoid, side_set_node_cnt_list, elem_ndx, 3); /* 3 node object */
            get_nodes(exoid, side_set_node_list, node_pos + 2, connect,
                      connect_offset + tri3_table[side_num][2] - 1);
          }
          else if (num_nodes_per_elem == 4) /* 4-node face */
          {
            set_count(exoid, side_set_node_cnt_list, elem_ndx, 4); /* 4 node object */
            get_nodes(exoid, side_set_node_list, node_pos + 2, connect,
                      connect_offset + tri3_table[side_num][2] - 1);
            get_nodes(exoid, side_set_node_list, node_pos + 2, connect,
                      connect_offset + 4 - 1); /* Center node of 4-noded face */
          }
          else if (num_nodes_per_elem == 6) /* 6-node face */
          {
            set_count(exoid, side_set_node_cnt_list, elem_ndx, 6); /* 6 node object */
            get_nodes(exoid, side_set_node_list, node_pos + 2, connect,
                      connect_offset + tri3_table[side_num][2] - 1);
            get_nodes(exoid, side_set_node_list, node_pos + 3, connect,
                      connect_offset + tri3_table[side_num][3] - 1);
            get_nodes(exoid, side_set_node_list, node_pos + 4, connect,
                      connect_offset + tri3_table[side_num][4] - 1);
            get_nodes(exoid, side_set_node_list, node_pos + 5, connect,
                      connect_offset + tri3_table[side_num][5] - 1);
          }
          else if (num_nodes_per_elem == 7) /* 7-node face */
          {
            set_count(exoid, side_set_node_cnt_list, elem_ndx, 7); /* 7 node object */
            get_nodes(exoid, side_set_node_list, node_pos + 2, connect,
                      connect_offset + tri3_table[side_num][2] - 1);
            get_nodes(exoid, side_set_node_list, node_pos + 3, connect,
                      connect_offset + tri3_table[side_num][3] - 1);
            get_nodes(exoid, side_set_node_list, node_pos + 4, connect,
                      connect_offset + tri3_table[side_num][4] - 1);
            get_nodes(exoid, side_set_node_list, node_pos + 5, connect,
                      connect_offset + tri3_table[side_num][5] - 1);
            get_nodes(exoid, side_set_node_list, node_pos + 6, connect,
                      connect_offset + tri3_table[side_num][6] - 1);
          }
          else {
            snprintf(errmsg, MAX_ERR_LENGTH,
                     "ERROR: %d is an unsupported number of nodes for the triangle element type",
                     (int)num_nodes_per_elem);
            ex_err("ex_get_side_set_node_list", errmsg, exerrval);
            err_stat = EX_FATAL;
            goto cleanup;
          }
        }
        else /* 2- or 3-node edge */
        {
          if (num_nodes_per_elem > 3) /* 3-node edge */
          {
            set_count(exoid, side_set_node_cnt_list, elem_ndx, 3); /* 3 node object */
            get_nodes(exoid, side_set_node_list, node_pos + 2, connect,
                      connect_offset + tri3_table[side_num][2] - 1);
          }
        }
      }
      break;
    }
    case EX_EL_QUAD: {
      if (check_valid_side(side_num, 4, "quad", exoid) != EX_NOERR) {
        goto cleanup;
      }

      get_nodes(exoid, side_set_node_list, node_pos + 0, connect,
                connect_offset + quad_table[side_num][0] - 1);
      get_nodes(exoid, side_set_node_list, node_pos + 1, connect,
                connect_offset + quad_table[side_num][1] - 1);
      set_count(exoid, side_set_node_cnt_list, elem_ndx, 2); /* 2 node object */
      if (num_nodes_per_elem > 5) {
        set_count(exoid, side_set_node_cnt_list, elem_ndx, 3); /* 3 node object */
        get_nodes(exoid, side_set_node_list, node_pos + 2, connect,
                  connect_offset + quad_table[side_num][2] - 1);
      }
      break;
    }
    case EX_EL_SHELL: {
      if (check_valid_side(side_num, 6, "shell", exoid) != EX_NOERR) {
        goto cleanup;
      }

      get_nodes(exoid, side_set_node_list, node_pos + 0, connect,
                connect_offset + shell_table[side_num][0] - 1);
      get_nodes(exoid, side_set_node_list, node_pos + 1, connect,
                connect_offset + shell_table[side_num][1] - 1);
      set_count(exoid, side_set_node_cnt_list, elem_ndx, 2);     /* 2 node object */
      if (num_nodes_per_elem > 2) {                              /*** KLUGE for 2D shells ***/
        if (side_num + 1 <= 2) {                                 /* 4-node face */
          set_count(exoid, side_set_node_cnt_list, elem_ndx, 4); /* 4 node object */
          get_nodes(exoid, side_set_node_list, node_pos + 2, connect,
                    connect_offset + shell_table[side_num][2] - 1);
          get_nodes(exoid, side_set_node_list, node_pos + 3, connect,
                    connect_offset + shell_table[side_num][3] - 1);
        }
      }
      if (num_nodes_per_elem == 8) {
        if (side_num + 1 <= 2) {                                 /* 8-node face */
          set_count(exoid, side_set_node_cnt_list, elem_ndx, 8); /* 8 node object */
          get_nodes(exoid, side_set_node_list, node_pos + 4, connect,
                    connect_offset + shell_table[side_num][4] - 1);
          get_nodes(exoid, side_set_node_list, node_pos + 5, connect,
                    connect_offset + shell_table[side_num][5] - 1);
          get_nodes(exoid, side_set_node_list, node_pos + 6, connect,
                    connect_offset + shell_table[side_num][6] - 1);
          get_nodes(exoid, side_set_node_list, node_pos + 7, connect,
                    connect_offset + shell_table[side_num][7] - 1);
        }
        else {
          set_count(exoid, side_set_node_cnt_list, elem_ndx, 3); /* 3 node edge */
          get_nodes(exoid, side_set_node_list, node_pos + 2, connect,
                    connect_offset + shell_table[side_num][2] - 1);
        }
      }
      if (num_nodes_per_elem == 9) {
        if (side_num + 1 <= 2) {                                 /* 9-node face */
          set_count(exoid, side_set_node_cnt_list, elem_ndx, 9); /* 9 node object */
          get_nodes(exoid, side_set_node_list, node_pos + 4, connect,
                    connect_offset + shell_table[side_num][4] - 1);
          get_nodes(exoid, side_set_node_list, node_pos + 5, connect,
                    connect_offset + shell_table[side_num][5] - 1);
          get_nodes(exoid, side_set_node_list, node_pos + 6, connect,
                    connect_offset + shell_table[side_num][6] - 1);
          get_nodes(exoid, side_set_node_list, node_pos + 7, connect,
                    connect_offset + shell_table[side_num][7] - 1);
          get_nodes(exoid, side_set_node_list, node_pos + 8, connect,
                    connect_offset + shell_table[side_num][8] - 1);
        }
        else {
          set_count(exoid, side_set_node_cnt_list, elem_ndx, 3); /* 3 node edge */
          get_nodes(exoid, side_set_node_list, node_pos + 2, connect,
                    connect_offset + shell_table[side_num][2] - 1);
        }
      }
      break;
    }
    case EX_EL_TETRA: {
      if (check_valid_side(side_num, 4, "tetra", exoid) != EX_NOERR) {
        goto cleanup;
      }

      get_nodes(exoid, side_set_node_list, node_pos + 0, connect,
                connect_offset + tetra_table[side_num][0] - 1);
      get_nodes(exoid, side_set_node_list, node_pos + 1, connect,
                connect_offset + tetra_table[side_num][1] - 1);
      get_nodes(exoid, side_set_node_list, node_pos + 2, connect,
                connect_offset + tetra_table[side_num][2] - 1);
      set_count(exoid, side_set_node_cnt_list, elem_ndx, 3); /* 3 node object */
      if (num_nodes_per_elem == 8) {
        set_count(exoid, side_set_node_cnt_list, elem_ndx, 4); /* 4 node object */
        get_nodes(exoid, side_set_node_list, node_pos + 3, connect,
                  connect_offset + tetra_table[side_num][3] - 1);
      }
      else if (num_nodes_per_elem > 8) {
        set_count(exoid, side_set_node_cnt_list, elem_ndx, 6); /* 6 node object */
        get_nodes(exoid, side_set_node_list, node_pos + 3, connect,
                  connect_offset + tetra_table[side_num][3] - 1);
        get_nodes(exoid, side_set_node_list, node_pos + 4, connect,
                  connect_offset + tetra_table[side_num][4] - 1);
        get_nodes(exoid, side_set_node_list, node_pos + 5, connect,
                  connect_offset + tetra_table[side_num][5] - 1);
      }
      break;
    }
    case EX_EL_WEDGE: {
      int node_off = 0;
      if (check_valid_side(side_num, 5, "wedge", exoid) != EX_NOERR) {
        goto cleanup;
      }

      /* All configurations and sides have at least 3 nodes per side */
      get_nodes(exoid, side_set_node_list, node_pos++, connect,
                connect_offset + wedge_table[side_num][node_off++] - 1);
      get_nodes(exoid, side_set_node_list, node_pos++, connect,
                connect_offset + wedge_table[side_num][node_off++] - 1);
      get_nodes(exoid, side_set_node_list, node_pos++, connect,
                connect_offset + wedge_table[side_num][node_off++] - 1);

      if (side_num == 3 || side_num == 4) {
        /* This is one of the triangular faces */
        set_count(exoid, side_set_node_cnt_list, elem_ndx, 3); /* 3 node side */
      }
      else {
        get_nodes(exoid, side_set_node_list, node_pos++, connect,
                  connect_offset + wedge_table[side_num][node_off++] - 1);
        set_count(exoid, side_set_node_cnt_list, elem_ndx, 4); /* 4 node side */
      }

      if (num_nodes_per_elem == 18) {
        /* Wedge 18 - 9-node quad faces (0,1,2) and 6-node tri faces (3,4) */
        /* All faces (quad or tri) have at least 6 nodes */
        /* This gets nodes 4,5,6 for tri or 5,6,7 for quad */
        get_nodes(exoid, side_set_node_list, node_pos++, connect,
                  connect_offset + wedge18_table[side_num][node_off++] - 1);
        get_nodes(exoid, side_set_node_list, node_pos++, connect,
                  connect_offset + wedge18_table[side_num][node_off++] - 1);
        get_nodes(exoid, side_set_node_list, node_pos++, connect,
                  connect_offset + wedge18_table[side_num][node_off++] - 1);

        if (side_num == 3 || side_num == 4) {
          /* This is a 9-node quad face */
          get_nodes(exoid, side_set_node_list, node_pos++, connect,
                    connect_offset + wedge18_table[side_num][node_off++] - 1);
          get_nodes(exoid, side_set_node_list, node_pos++, connect,
                    connect_offset + wedge18_table[side_num][node_off++] - 1);
          set_count(exoid, side_set_node_cnt_list, elem_ndx, 9); /* 9 node side */
        }
        else {
          set_count(exoid, side_set_node_cnt_list, elem_ndx, 6); /* 6 node side */
        }
      }

      else if (num_nodes_per_elem >= 15) {
        /* Wedge 15 - 8-node quad faces (0,1,2) and 6-node tri faces (3,4) */
        /* Wedge 20 - 9-node quad faces (0,1,2) and 7-node tri faces (3,4) */
        /* Wedge 21 - 9-node quad faces (0,1,2) and 7-node tri faces (3,4) + center */

        /* All faces (quad or tri) have at least 6 nodes */
        /* This gets nodes 4,5,6 for tri or 5,6,7 for quad */
        get_nodes(exoid, side_set_node_list, node_pos++, connect,
                  connect_offset + wedge_table[side_num][node_off++] - 1);
        get_nodes(exoid, side_set_node_list, node_pos++, connect,
                  connect_offset + wedge_table[side_num][node_off++] - 1);
        get_nodes(exoid, side_set_node_list, node_pos++, connect,
                  connect_offset + wedge_table[side_num][node_off++] - 1);

        if (side_num == 3 || side_num == 4) {
          /* This is a quad face -- either 8-node or 9-node */
          get_nodes(exoid, side_set_node_list, node_pos++, connect,
                    connect_offset + wedge_table[side_num][node_off++] - 1);
          if (num_nodes_per_elem == 20 || num_nodes_per_elem == 21) {
            get_nodes(exoid, side_set_node_list, node_pos++, connect,
                      connect_offset + wedge_table[side_num][node_off++] - 1);
            set_count(exoid, side_set_node_cnt_list, elem_ndx, 9); /* 9 node side */
          }
          else {
            set_count(exoid, side_set_node_cnt_list, elem_ndx, 8); /* 8 node side */
          }
        }
        else {
          /* This is a tri face -- either 6-node or 7-node */
          if (num_nodes_per_elem == 20 || num_nodes_per_elem == 21) {
            get_nodes(exoid, side_set_node_list, node_pos++, connect,
                      connect_offset + wedge_table[side_num][node_off++] - 1);
            set_count(exoid, side_set_node_cnt_list, elem_ndx, 7); /* 7 node side */
          }
          else {
            set_count(exoid, side_set_node_cnt_list, elem_ndx, 6); /* 6 node side */
          }
        }
      }
      break;
    }
    case EX_EL_PYRAMID: {
      if (check_valid_side(side_num, 5, "pyramid", exoid) != EX_NOERR) {
        goto cleanup;
      }

      get_nodes(exoid, side_set_node_list, node_pos++, connect,
                connect_offset + pyramid_table[side_num][0] - 1);
      get_nodes(exoid, side_set_node_list, node_pos++, connect,
                connect_offset + pyramid_table[side_num][1] - 1);
      get_nodes(exoid, side_set_node_list, node_pos++, connect,
                connect_offset + pyramid_table[side_num][2] - 1);

      if (pyramid_table[side_num][3] == 0) {                   /* degenerate side? */
        set_count(exoid, side_set_node_cnt_list, elem_ndx, 3); /* 3 node side */
      }
      else {
        get_nodes(exoid, side_set_node_list, node_pos++, connect,
                  connect_offset + pyramid_table[side_num][3] - 1);
        set_count(exoid, side_set_node_cnt_list, elem_ndx, 4); /* 4 node side */
      }

      if (num_nodes_per_elem > 5) {
        get_nodes(exoid, side_set_node_list, node_pos++, connect,
                  connect_offset + pyramid_table[side_num][4] - 1);
        get_nodes(exoid, side_set_node_list, node_pos++, connect,
                  connect_offset + pyramid_table[side_num][5] - 1);
        get_nodes(exoid, side_set_node_list, node_pos++, connect,
                  connect_offset + pyramid_table[side_num][6] - 1);

        if (pyramid_table[side_num][7] == 0) {                   /* degenerate side? */
          set_count(exoid, side_set_node_cnt_list, elem_ndx, 6); /* 6 node side */
        }
        else {
          get_nodes(exoid, side_set_node_list, node_pos++, connect,
                    connect_offset + pyramid_table[side_num][7] - 1);
          set_count(exoid, side_set_node_cnt_list, elem_ndx, 8); /* 8 node side */
        }
      }
      break;
    }
    case EX_EL_HEX: {
      if (check_valid_side(side_num, 6, "hex", exoid) != EX_NOERR) {
        goto cleanup;
      }

      get_nodes(exoid, side_set_node_list, node_pos + 0, connect,
                connect_offset + hex_table[side_num][0] - 1);
      get_nodes(exoid, side_set_node_list, node_pos + 1, connect,
                connect_offset + hex_table[side_num][1] - 1);
      get_nodes(exoid, side_set_node_list, node_pos + 2, connect,
                connect_offset + hex_table[side_num][2] - 1);
      get_nodes(exoid, side_set_node_list, node_pos + 3, connect,
                connect_offset + hex_table[side_num][3] - 1);
      set_count(exoid, side_set_node_cnt_list, elem_ndx, 4); /* 4 node object */
      if (num_nodes_per_elem > 12)                           /* more nodes than HEXSHELL */
      {
        set_count(exoid, side_set_node_cnt_list, elem_ndx, 8); /* 8 node object */
        get_nodes(exoid, side_set_node_list, node_pos + 4, connect,
                  connect_offset + hex_table[side_num][4] - 1);
        get_nodes(exoid, side_set_node_list, node_pos + 5, connect,
                  connect_offset + hex_table[side_num][5] - 1);
        get_nodes(exoid, side_set_node_list, node_pos + 6, connect,
                  connect_offset + hex_table[side_num][6] - 1);
        get_nodes(exoid, side_set_node_list, node_pos + 7, connect,
                  connect_offset + hex_table[side_num][7] - 1);
      }
      if (num_nodes_per_elem == 27) /* 27-node brick */
      {
        set_count(exoid, side_set_node_cnt_list, elem_ndx, 9); /* 9 node object */
        get_nodes(exoid, side_set_node_list, node_pos + 8, connect,
                  connect_offset + hex_table[side_num][8] - 1);
      }
      break;
    }
    default: {
      exerrval = EX_BADPARAM;
      snprintf(errmsg, MAX_ERR_LENGTH, "ERROR: %s is an unsupported element type",
               elem_blk_parms[parm_ndx].elem_type);
      ex_err("ex_get_side_set_node_list", errmsg, exerrval);
      err_stat = EX_FATAL;
      goto cleanup;
    }
    }
  }

/* All done: release connectivity array space, element block ids array,
   element block parameters array, and side set element index array */
cleanup:
  ex_safe_free(connect);
  ex_safe_free(ss_parm_ndx);
  ex_safe_free(elem_blk_ids);
  ex_safe_free(elem_blk_parms);
  ex_safe_free(ss_elem_ndx);
  ex_safe_free(ss_elem_node_ndx);
  ex_safe_free(side_set_side_list);
  ex_safe_free(side_set_elem_list);

  return (err_stat);
}
