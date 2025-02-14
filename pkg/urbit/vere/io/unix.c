/* vere/unix.c
**
**  this file is responsible for maintaining a bidirectional
**  mapping between the contents of a clay desk and a directory
**  in a unix filesystem.
**
**  TODO  this driver is crufty and overdue for a rewrite.
**  aspirationally, the rewrite should do sanity checking and
**  transformations at the noun level to convert messages from
**  arvo into sets of fs operations on trusted inputs, and
**  inverse transformations and checks for fs contents to arvo
**  messages.
**
**  the two relevant transformations to apply are:
**
**  1. bidirectionally map file contents to atoms
**  2. bidirectionally map arvo $path <-> unix relative paths
**
**  the first transform is trivial. the second poses some
**  challenges: an arvo $path is a list of $knot, and the $knot
**  space intersects with invalid unix paths in the three cases
**  of: %$ (the empty knot), '.', and '..'. we escape these by
**  prepending a '!' to the filename corresponding to the $knot,
**  yielding unix files named '!', '!.', and '!..'.
**
**  there is also the case of the empty path. we elide empty
**  paths from this wrapper, which always uses the last path
**  component as the file extension/mime-type.
**
**  these transforms are implemented, but they ought to be
**  implemented in one place, prior to any fs calls; as-is, they
**  are sprinkled throughout the file updating code.
**
*/
#include "all.h"
#include <ftw.h>
#include "vere/vere.h"

struct _u3_umon;
struct _u3_udir;
struct _u3_ufil;

/* u3_unod: file or directory.
*/
  typedef struct _u3_unod {
    c3_o              dir;              //  c3y if dir, c3n if file
    c3_o              dry;              //  ie, unmodified
    c3_c*             pax_c;            //  absolute path
    struct _u3_udir*  par_u;            //  parent
    struct _u3_unod*  nex_u;            //  internal list
  } u3_unod;

/* u3_ufil: synchronized file.
*/
  typedef struct _u3_ufil {
    c3_o              dir;              //  c3y if dir, c3n if file
    c3_o              dry;              //  ie, unmodified
    c3_c*             pax_c;            //  absolute path
    struct _u3_udir*  par_u;            //  parent
    struct _u3_unod*  nex_u;            //  internal list
    c3_w              gum_w;            //  mug of last %ergo
  } u3_ufil;

/* u3_ufil: synchronized directory.
*/
  typedef struct _u3_udir {
    c3_o              dir;              //  c3y if dir, c3n if file
    c3_o              dry;              //  ie, unmodified
    c3_c*             pax_c;            //  absolute path
    struct _u3_udir*  par_u;            //  parent
    struct _u3_unod*  nex_u;            //  internal list
    u3_unod*          kid_u;            //  subnodes
  } u3_udir;

/* u3_ufil: synchronized mount point.
*/
  typedef struct _u3_umon {
    u3_udir          dir_u;             //  root directory, must be first
    c3_c*            nam_c;             //  mount point name
    struct _u3_umon* nex_u;             //  internal list
  } u3_umon;

/* u3_unix: clay support system, also
*/
  typedef struct _u3_unix {
    u3_auto     car_u;
    c3_l        sev_l;                  //  instance number
    u3_umon*    mon_u;                  //  mount points
    c3_c*       pax_c;                  //  pier directory
    c3_o        alm;                    //  timer set
    c3_o        dyr;                    //  ready to update
    u3_noun     sat;                    //  (sane %ta) handle
#ifdef SYNCLOG
    c3_w         lot_w;                 //  sync-slot
    struct _u3_sylo {
      c3_o     unx;                     //  from unix
      c3_m     wer_m;                   //  mote saying where
      c3_m     wot_m;                   //  mote saying what
      c3_c*    pax_c;                   //  path
    } sylo[1024];
#endif
  } u3_unix;

void
u3_unix_ef_look(u3_unix* unx_u, u3_noun mon, u3_noun all);

/* u3_unix_cane(): true iff (unix) path is canonical.
*/
c3_t
u3_unix_cane(const c3_c* pax_c)
{
  if ( 0 == pax_c ) {
    return 0;
  }
  //  allow absolute paths.
  //
  if ( '/' == *pax_c ) {
    pax_c++;
    //  allow root.
    //
    if ( 0 == *pax_c ) {
      return 1;
    }
  }
  do {
    if (  0 == *pax_c
       || 0 == strcmp(".",    pax_c)
       || 0 == strcmp("..",   pax_c)
       || 0 == strncmp("/",   pax_c, 1)
       || 0 == strncmp("./",  pax_c, 2)
       || 0 == strncmp("../", pax_c, 3) )
    {
      return 0;
    }
    pax_c = strchr(pax_c, '/');
  } while ( 0 != pax_c++ );
  return 1;
}

/* _unix_sane_ta(): true iff pat is a valid @ta
**
**  %ta is parsed by:
**      (star ;~(pose nud low hep dot sig cab))
*/
static c3_t
_unix_sane_ta(u3_unix* unx_u, u3_atom pat)
{
  return _(u3n_slam_on(u3k(unx_u->sat), pat));
}

/* u3_readdir_r():
*/
c3_w
u3_readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
  errno = 0;
  struct dirent * tmp_u = readdir(dirp);

  if (NULL == tmp_u){
    *result = NULL;
    return (errno);  // either success or error code
  } else {
    memcpy(entry, tmp_u, sizeof(struct dirent));
    *result = entry;
  }

  return(0);
}

/* _unix_string_to_knot(): convert c unix path component to $knot
*/
static u3_atom
_unix_string_to_knot(c3_c* pax_c)
{
  c3_assert(pax_c);
  //  XX  this can happen if we encounter a file without an extension.
  //
  // c3_assert(*pax_c);
  c3_assert(!strchr(pax_c, '/'));
  //  XX  horrible
  //
# ifdef _WIN32
  c3_assert(!strchr(pax_c, '\\'));
# endif
  if ( '!' == *pax_c ) {
    pax_c++;
  }
  return u3i_string(pax_c);
}

/* _unix_knot_to_string(): convert $knot to c unix path component. RETAIN.
*/
static c3_c*
_unix_knot_to_string(u3_atom pon)
{
  c3_c* ret_c;

  if (  u3_nul != pon
     && c3_s1('.') != pon
     && c3_s2('.','.') != pon
     && '!' != u3r_byte(0, pon) )
  {
    ret_c = u3r_string(pon);
  }
  else {
    c3_w  met_w = u3r_met(3, pon);

    ret_c = c3_malloc(met_w + 2);
    *ret_c = '!';
    u3r_bytes(0, met_w, (c3_y*)ret_c + 1, pon);
    ret_c[met_w + 1] = 0;
  }
  c3_assert(!strchr(ret_c, '/'));
# ifdef _WIN32
  c3_assert(!strchr(ret_c, '\\'));
# endif
  return ret_c;
}

/* _unix_down(): descend path.
*/
static c3_c*
_unix_down(c3_c* pax_c, c3_c* sub_c)
{
  c3_w pax_w = strlen(pax_c);
  c3_w sub_w = strlen(sub_c);
  c3_c* don_c = c3_malloc(pax_w + sub_w + 2);

  strcpy(don_c, pax_c);
  don_c[pax_w] = '/';
  strcpy(don_c + pax_w + 1, sub_c);
  don_c[pax_w + 1 + sub_w] = '\0';

  return don_c;
}

/* _unix_string_to_path(): convert c string to u3_noun $path
**
**  c string must begin with the pier path plus mountpoint
*/
static u3_noun
_unix_string_to_path_helper(c3_c* pax_c)
{
  u3_noun not;

  c3_assert(pax_c[-1] == '/');
  c3_c* end_c = strchr(pax_c, '/');
  if ( !end_c ) {
    end_c = strrchr(pax_c, '.');
    if ( !end_c ) {
      return u3nc(_unix_string_to_knot(pax_c), u3_nul);
    }
    else {
      *end_c = 0;
      not = _unix_string_to_knot(pax_c);
      *end_c = '.';
      return u3nt(not, _unix_string_to_knot(end_c + 1), u3_nul);
    }
  }
  else {
    *end_c = 0;
    not = _unix_string_to_knot(pax_c);
    *end_c = '/';
    return u3nc(not, _unix_string_to_path_helper(end_c + 1));
  }
}
static u3_noun
_unix_string_to_path(u3_unix* unx_u, c3_c* pax_c)
{
  pax_c += strlen(unx_u->pax_c) + 1;
  c3_c* pox_c = strchr(pax_c, '/');
  if ( !pox_c ) {
    pox_c = strchr(pax_c, '.');
    if ( !pox_c ) {
      return u3_nul;
    }
    else {
      return u3nc(_unix_string_to_knot(pox_c + 1), u3_nul);
    }
  }
  else {
    return _unix_string_to_path_helper(pox_c + 1);
  }
}

/* _unix_mkdirp(): recursive mkdir of dirname of pax_c.
*/
static void
_unix_mkdirp(c3_c* pax_c)
{
  c3_c* fas_c = strchr(pax_c + 1, '/');

  while ( fas_c ) {
    *fas_c = 0;
    if ( 0 != mkdir(pax_c, 0777) && EEXIST != errno ) {
      u3l_log("unix: mkdir %s: %s", pax_c, strerror(errno));
      u3m_bail(c3__fail);
    }
    *fas_c++ = '/';
    fas_c = strchr(fas_c, '/');
  }
}

/* u3_unix_save(): save file under .../.urb/put or bail.
**
**  XX this is quite bad, and doesn't share much in common with
**  the rest of unix.c. a refactor would probably share common
**  logic with _unix_sync_change, perhaps using openat, making
**  unx_u optional, and/or having a flag to not track the file
**  for future changes.
*/
void
u3_unix_save(c3_c* pax_c, u3_atom pad)
{
  c3_i  fid_i;
  c3_w  lod_w, len_w, fln_w, rit_w;
  c3_y* pad_y;
  c3_c* ful_c;

  if ( !u3_unix_cane(pax_c) ) {
    u3l_log("%s: non-canonical path", pax_c);
    u3z(pad); u3m_bail(c3__fail);
  }
  if ( '/' == *pax_c) {
    pax_c++;
  }
  lod_w = strlen(u3_Host.dir_c);
  len_w = lod_w + sizeof("/.urb/put/") + strlen(pax_c);
  ful_c = c3_malloc(len_w);
  rit_w = snprintf(ful_c, len_w, "%s/.urb/put/%s", u3_Host.dir_c, pax_c);
  c3_assert(len_w == rit_w + 1);

  _unix_mkdirp(ful_c);
  fid_i = c3_open(ful_c, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if ( fid_i < 0 ) {
    u3l_log("%s: %s", ful_c, strerror(errno));
    c3_free(ful_c);
    u3z(pad); u3m_bail(c3__fail);
  }

  fln_w = u3r_met(3, pad);
  pad_y = c3_malloc(fln_w);
  u3r_bytes(0, fln_w, pad_y, pad);
  u3z(pad);
  rit_w = write(fid_i, pad_y, fln_w);
  close(fid_i);
  c3_free(pad_y);

  if ( rit_w != fln_w ) {
    u3l_log("%s: %s", ful_c, strerror(errno));
    c3_free(ful_c);
    u3m_bail(c3__fail);
  }
  c3_free(ful_c);
}

/* _unix_rm_r_cb(): callback to delete individual files/directories
*/
static c3_i
_unix_rm_r_cb(const c3_c* pax_c,
              const struct stat* buf_u,
              c3_i typeflag,
              struct FTW* ftw_u)
{
  switch ( typeflag ) {
    default:
      u3l_log("bad file type in rm_r: %s", pax_c);
      break;
    case FTW_F:
      if ( 0 != c3_unlink(pax_c) && ENOENT != errno ) {
        u3l_log("error unlinking (in rm_r) %s: %s",
                pax_c, strerror(errno));
        c3_assert(0);
      }
      break;
    case FTW_D:
      u3l_log("shouldn't have gotten pure directory: %s", pax_c);
      break;
    case FTW_DNR:
      u3l_log("couldn't read directory: %s", pax_c);
      break;
    case FTW_NS:
      u3l_log("couldn't stat path: %s", pax_c);
      break;
    case FTW_DP:
      if ( 0 != c3_rmdir(pax_c) && ENOENT != errno ) {
        u3l_log("error rmdiring %s: %s", pax_c, strerror(errno));
        c3_assert(0);
      }
      break;
    case FTW_SL:
      u3l_log("got symbolic link: %s", pax_c);
      break;
    case FTW_SLN:
      u3l_log("got nonexistent symbolic link: %s", pax_c);
      break;
  }

  return 0;
}

/* _unix_rm_r(): rm -r directory
*/
static void
_unix_rm_r(c3_c* pax_c)
{
  if ( 0 > nftw(pax_c, _unix_rm_r_cb, 100, FTW_DEPTH | FTW_PHYS )
       && ENOENT != errno) {
    u3l_log("rm_r error on %s: %s", pax_c, strerror(errno));
  }
}

/* _unix_mkdir(): mkdir, asserting.
*/
static void
_unix_mkdir(c3_c* pax_c)
{
  if ( 0 != c3_mkdir(pax_c, 0755) && EEXIST != errno) {
    u3l_log("error mkdiring %s: %s", pax_c, strerror(errno));
    c3_assert(0);
  }
}

/* _unix_write_file_hard(): write to a file, overwriting what's there
*/
static c3_w
_unix_write_file_hard(c3_c* pax_c, u3_noun mim)
{
  c3_i  fid_i = c3_open(pax_c, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  c3_w  len_w, rit_w, siz_w, mug_w = 0;
  c3_y* dat_y;

  u3_noun dat = u3t(u3t(mim));

  if ( fid_i < 0 ) {
    u3l_log("error opening %s for writing: %s",
            pax_c, strerror(errno));
    u3z(mim);
    return 0;
  }

  siz_w = u3h(u3t(mim));
  len_w = u3r_met(3, dat);
  dat_y = c3_calloc(siz_w);

  u3r_bytes(0, len_w, dat_y, dat);
  u3z(mim);

  rit_w = write(fid_i, dat_y, siz_w);

  if ( rit_w != siz_w ) {
    u3l_log("error writing %s: %s",
            pax_c, strerror(errno));
    mug_w = 0;
  }
  else {
    mug_w = u3r_mug_bytes(dat_y, len_w);
  }

  close(fid_i);
  c3_free(dat_y);

  return mug_w;
}

/* _unix_write_file_soft(): write to a file, not overwriting if it's changed
*/
static void
_unix_write_file_soft(u3_ufil* fil_u, u3_noun mim)
{
  struct stat buf_u;
  c3_i  fid_i = c3_open(fil_u->pax_c, O_RDONLY, 0644);
  c3_ws len_ws, red_ws;
  c3_w  old_w;
  c3_y* old_y;

  if ( fid_i < 0 || fstat(fid_i, &buf_u) < 0 ) {
    if ( ENOENT == errno ) {
      goto _unix_write_file_soft_go;
    }
    else {
      u3l_log("error opening file (soft) %s: %s",
              fil_u->pax_c, strerror(errno));
      u3z(mim);
      return;
    }
  }

  len_ws = buf_u.st_size;
  old_y = c3_malloc(len_ws);

  red_ws = read(fid_i, old_y, len_ws);

  if ( close(fid_i) < 0 ) {
    u3l_log("error closing file (soft) %s: %s",
            fil_u->pax_c, strerror(errno));
  }

  if ( len_ws != red_ws ) {
    if ( red_ws < 0 ) {
      u3l_log("error reading file (soft) %s: %s",
              fil_u->pax_c, strerror(errno));
    }
    else {
      u3l_log("wrong # of bytes read in file %s: %d %d",
              fil_u->pax_c, len_ws, red_ws);
    }
    c3_free(old_y);
    u3z(mim);
    return;
  }

  old_w = u3r_mug_bytes(old_y, len_ws);

  if ( old_w != fil_u->gum_w ) {
    fil_u->gum_w = u3r_mug(u3t(u3t(mim))); // XXX this might fail with
    c3_free(old_y);                           //     trailing zeros
    u3z(mim);
    return;
  }

  c3_free(old_y);

_unix_write_file_soft_go:
  fil_u->gum_w = _unix_write_file_hard(fil_u->pax_c, mim);
}

static void
_unix_watch_dir(u3_udir* dir_u, u3_udir* par_u, c3_c* pax_c);
static void
_unix_watch_file(u3_unix* unx_u, u3_ufil* fil_u, u3_udir* par_u, c3_c* pax_c);

/* _unix_get_mount_point(): retrieve or create mount point
*/
static u3_umon*
_unix_get_mount_point(u3_unix* unx_u, u3_noun mon)
{
  if ( c3n == u3ud(mon) ) {
    c3_assert(!"mount point must be an atom");
    u3z(mon);
    return NULL;
  }

  c3_c* nam_c = _unix_knot_to_string(mon);
  u3_umon* mon_u;

  for ( mon_u = unx_u->mon_u;
        mon_u && 0 != strcmp(nam_c, mon_u->nam_c);
        mon_u = mon_u->nex_u )
  {
  }

  if ( !mon_u ) {
    mon_u = c3_malloc(sizeof(u3_umon));
    mon_u->nam_c = nam_c;
    mon_u->dir_u.dir = c3y;
    mon_u->dir_u.dry = c3n;
    mon_u->dir_u.pax_c = strdup(unx_u->pax_c);
    mon_u->dir_u.par_u = NULL;
    mon_u->dir_u.nex_u = NULL;
    mon_u->dir_u.kid_u = NULL;
    mon_u->nex_u = unx_u->mon_u;
    unx_u->mon_u = mon_u;
  }
  else {
    c3_free(nam_c);
  }

  u3z(mon);

  return mon_u;
}

/* _unix_scan_mount_point(): scan unix for already-existing mount point
*/
static void
_unix_scan_mount_point(u3_unix* unx_u, u3_umon* mon_u)
{
  DIR* rid_u = c3_opendir(mon_u->dir_u.pax_c);
  if ( !rid_u ) {
    u3l_log("error opening pier directory: %s: %s",
            mon_u->dir_u.pax_c, strerror(errno));
    return;
  }

  c3_w len_w = strlen(mon_u->nam_c);

  while ( 1 ) {
    struct dirent  ent_u;
    struct dirent* out_u;
    c3_w err_w;

    if ( 0 != (err_w = u3_readdir_r(rid_u, &ent_u, &out_u)) ) {
      u3l_log("erroring loading pier directory %s: %s",
              mon_u->dir_u.pax_c, strerror(errno));

      c3_assert(0);
    }
    else if ( !out_u ) {
      break;
    }
    else if ( '.' == out_u->d_name[0] ) { // unnecessary, but consistency
      continue;
    }
    else if ( 0 != strncmp(mon_u->nam_c, out_u->d_name, len_w) ) {
      continue;
    }
    else {
      c3_c* pax_c = _unix_down(mon_u->dir_u.pax_c, out_u->d_name);

      struct stat buf_u;

      if ( 0 != stat(pax_c, &buf_u) ) {
        u3l_log("can't stat pier directory %s: %s",
                mon_u->dir_u.pax_c, strerror(errno));
        c3_free(pax_c);
        continue;
      }
      if ( S_ISDIR(buf_u.st_mode) ) {
        if ( out_u->d_name[len_w] != '\0' ) {
          c3_free(pax_c);
          continue;
        }
        else {
          u3_udir* dir_u = c3_malloc(sizeof(u3_udir));
          _unix_watch_dir(dir_u, &mon_u->dir_u, pax_c);
        }
      }
      else {
        if (  '.'  != out_u->d_name[len_w]
           || '\0' == out_u->d_name[len_w + 1]
           || '~'  == out_u->d_name[strlen(out_u->d_name) - 1]
           || !_unix_sane_ta(unx_u, _unix_string_to_knot(out_u->d_name)) )
        {
          c3_free(pax_c);
          continue;
        }
        else {
          u3_ufil* fil_u = c3_malloc(sizeof(u3_ufil));
          _unix_watch_file(unx_u, fil_u, &mon_u->dir_u, pax_c);
        }
      }

      c3_free(pax_c);
    }
  }
}

static u3_noun _unix_free_node(u3_unix* unx_u, u3_unod* nod_u);

/* _unix_free_file(): free file, unlinking it
*/
static void
_unix_free_file(u3_ufil *fil_u)
{
  if ( 0 != c3_unlink(fil_u->pax_c) && ENOENT != errno ) {
    u3l_log("error unlinking %s: %s", fil_u->pax_c, strerror(errno));
    c3_assert(0);
  }

  c3_free(fil_u->pax_c);
  c3_free(fil_u);
}

/* _unix_free_dir(): free directory, deleting everything within
*/
static void
_unix_free_dir(u3_udir *dir_u)
{
  _unix_rm_r(dir_u->pax_c);

  if ( dir_u->kid_u ) {
    fprintf(stderr, "don't kill me, i've got a family %s\r\n", dir_u->pax_c);
  }
  else {
    // fprintf(stderr, "i'm a lone, lonely loner %s\r\n", dir_u->pax_c);
  }
  c3_free(dir_u->pax_c);
  c3_free(dir_u); // XXX this might be too early, how do we
               //     know we've freed all the children?
               //     i suspect we should do this only if
               //     our kid list is empty
}

/* _unix_free_node(): free node, deleting everything within
**
**  also deletes from parent list if in it
*/
static u3_noun
_unix_free_node(u3_unix* unx_u, u3_unod* nod_u)
{
  u3_noun can;
  if ( nod_u->par_u ) {
    u3_unod* don_u = nod_u->par_u->kid_u;

    if ( !don_u ) {
    }
    else if ( nod_u == don_u ) {
      nod_u->par_u->kid_u = nod_u->par_u->kid_u->nex_u;
    }
    else {
      for ( ; don_u->nex_u && nod_u != don_u->nex_u; don_u = don_u->nex_u ) {
      }
      if ( don_u->nex_u ) {
        don_u->nex_u = don_u->nex_u->nex_u;
      }
    }
  }

  if ( c3y == nod_u->dir ) {
    can = u3_nul;
    u3_unod* nud_u = ((u3_udir*) nod_u)->kid_u;
    while ( nud_u ) {
      u3_unod* nex_u = nud_u->nex_u;
      can = u3kb_weld(_unix_free_node(unx_u, nud_u), can);
      nud_u = nex_u;
    }
    _unix_free_dir((u3_udir *)nod_u);
  }
  else {
    can = u3nc(u3nc(_unix_string_to_path(unx_u, nod_u->pax_c), u3_nul),
               u3_nul);
    _unix_free_file((u3_ufil *)nod_u);
  }

  return can;
}

/* _unix_free_mount_point(): free mount point
**
**  this process needs to happen in a very careful order. in
**  particular, we must recurse before we get to the callback, so
**  that libuv does all the child directories before it does us.
**
**  tread carefully
*/
static void
_unix_free_mount_point(u3_unix* unx_u, u3_umon* mon_u)
{
  u3_unod* nod_u;
  for ( nod_u = mon_u->dir_u.kid_u; nod_u; ) {
    u3_unod* nex_u = nod_u->nex_u;
    u3z(_unix_free_node(unx_u, nod_u));
    nod_u = nex_u;
  }

  c3_free(mon_u->dir_u.pax_c);
  c3_free(mon_u->nam_c);
  c3_free(mon_u);
}

/* _unix_delete_mount_point(): remove mount point from list and free
*/
static void
_unix_delete_mount_point(u3_unix* unx_u, u3_noun mon)
{
  if ( c3n == u3ud(mon) ) {
    c3_assert(!"mount point must be an atom");
    u3z(mon);
    return;
  }

  c3_c* nam_c = _unix_knot_to_string(mon);
  u3_umon* mon_u;
  u3_umon* tem_u;

  mon_u = unx_u->mon_u;
  if ( !mon_u ) {
    u3l_log("mount point already gone: %s", nam_c);
    goto _delete_mount_point_out;
  }
  if ( 0 == strcmp(nam_c, mon_u->nam_c) ) {
    unx_u->mon_u = mon_u->nex_u;
    _unix_free_mount_point(unx_u, mon_u);
    goto _delete_mount_point_out;
  }

  for ( ;
        mon_u->nex_u && 0 != strcmp(nam_c, mon_u->nex_u->nam_c);
        mon_u = mon_u->nex_u )
  {
  }

  if ( !mon_u->nex_u ) {
    u3l_log("mount point already gone: %s", nam_c);
    goto _delete_mount_point_out;
  }

  tem_u = mon_u->nex_u;
  mon_u->nex_u = mon_u->nex_u->nex_u;
  _unix_free_mount_point(unx_u, tem_u);

_delete_mount_point_out:
  c3_free(nam_c);
  u3z(mon);
}

/* _unix_commit_mount_point: commit from mount point
*/
static void
_unix_commit_mount_point(u3_unix* unx_u, u3_noun mon)
{
  unx_u->dyr = c3y;
  u3_unix_ef_look(unx_u, mon, c3n);
  return;
}

/* _unix_watch_file(): initialize file
*/
static void
_unix_watch_file(u3_unix* unx_u, u3_ufil* fil_u, u3_udir* par_u, c3_c* pax_c)
{
  // initialize fil_u

  fil_u->dir = c3n;
  fil_u->dry = c3n;
  fil_u->pax_c = c3_malloc(1 + strlen(pax_c));
  strcpy(fil_u->pax_c, pax_c);
  fil_u->par_u = par_u;
  fil_u->nex_u = NULL;
  fil_u->gum_w = 0;

  if ( par_u ) {
    fil_u->nex_u = par_u->kid_u;
    par_u->kid_u = (u3_unod*) fil_u;
  }
}

/* _unix_watch_dir(): initialize directory
*/
static void
_unix_watch_dir(u3_udir* dir_u, u3_udir* par_u, c3_c* pax_c)
{
  // initialize dir_u

  dir_u->dir = c3y;
  dir_u->dry = c3n;
  dir_u->pax_c = c3_malloc(1 + strlen(pax_c));
  strcpy(dir_u->pax_c, pax_c);
  dir_u->par_u = par_u;
  dir_u->nex_u = NULL;
  dir_u->kid_u = NULL;

  if ( par_u ) {
    dir_u->nex_u = par_u->kid_u;
    par_u->kid_u = (u3_unod*) dir_u;
  }
}

/* _unix_create_dir(): create unix directory and watch it
*/
static void
_unix_create_dir(u3_udir* dir_u, u3_udir* par_u, u3_noun nam)
{
  c3_c* nam_c = _unix_knot_to_string(nam);
  c3_w  nam_w = strlen(nam_c);
  c3_w  pax_w = strlen(par_u->pax_c);
  c3_c* pax_c = c3_malloc(pax_w + 1 + nam_w + 1);

  strcpy(pax_c, par_u->pax_c);
  pax_c[pax_w] = '/';
  strcpy(pax_c + pax_w + 1, nam_c);
  pax_c[pax_w + 1 + nam_w] = '\0';

  c3_free(nam_c);
  u3z(nam);

  _unix_mkdir(pax_c);
  _unix_watch_dir(dir_u, par_u, pax_c);
}

static u3_noun _unix_update_node(u3_unix* unx_u, u3_unod* nod_u);

/* _unix_update_file(): update file, producing list of changes
**
**  when scanning through files, if dry, do nothing. otherwise,
**  mark as dry, then check if file exists. if not, remove
**  self from node list and add path plus sig to %into event.
**  otherwise, read the file and get a mug checksum. if same as
**  gum_w, move on. otherwise, overwrite add path plus data to
**  %into event.
*/
static u3_noun
_unix_update_file(u3_unix* unx_u, u3_ufil* fil_u)
{
  c3_assert( c3n == fil_u->dir );

  if ( c3y == fil_u->dry ) {
    return u3_nul;
  }

  fil_u->dry = c3n;

  struct stat buf_u;
  c3_i  fid_i = c3_open(fil_u->pax_c, O_RDONLY, 0644);
  c3_ws len_ws, red_ws;
  c3_y* dat_y;

  if ( fid_i < 0 || fstat(fid_i, &buf_u) < 0 ) {
    if ( ENOENT == errno ) {
      return u3nc(u3nc(_unix_string_to_path(unx_u, fil_u->pax_c), u3_nul), u3_nul);
    }
    else {
      u3l_log("error opening file %s: %s",
              fil_u->pax_c, strerror(errno));
      return u3_nul;
    }
  }

  len_ws = buf_u.st_size;
  dat_y = c3_malloc(len_ws);

  red_ws = read(fid_i, dat_y, len_ws);

  if ( close(fid_i) < 0 ) {
    u3l_log("error closing file %s: %s",
            fil_u->pax_c, strerror(errno));
  }

  if ( len_ws != red_ws ) {
    if ( red_ws < 0 ) {
      u3l_log("error reading file %s: %s",
              fil_u->pax_c, strerror(errno));
    }
    else {
      u3l_log("wrong # of bytes read in file %s: %d %d",
              fil_u->pax_c, len_ws, red_ws);
    }
    c3_free(dat_y);
    return u3_nul;
  }
  else {
    c3_w mug_w = u3r_mug_bytes(dat_y, len_ws);
    if ( mug_w == fil_u->gum_w ) {
      c3_free(dat_y);
      return u3_nul;
    }
    else {
      u3_noun pax = _unix_string_to_path(unx_u, fil_u->pax_c);
      u3_noun mim = u3nt(c3__text, u3i_string("plain"), u3_nul);
      u3_noun dat = u3nt(mim, len_ws, u3i_bytes(len_ws, dat_y));

      c3_free(dat_y);
      return u3nc(u3nt(pax, u3_nul, dat), u3_nul);
    }
  }
}

/* _unix_update_dir(): update directory, producing list of changes
**
**  when changing this, consider whether to also change
**  _unix_initial_update_dir()
*/
static u3_noun
_unix_update_dir(u3_unix* unx_u, u3_udir* dir_u)
{
  u3_noun can = u3_nul;

  c3_assert( c3y == dir_u->dir );

  if ( c3y == dir_u->dry ) {
    return u3_nul;
  }

  dir_u->dry = c3n;

  // Check that old nodes are still there

  u3_unod* nod_u = dir_u->kid_u;

  if ( nod_u ) {
    while ( nod_u ) {
      if ( c3y == nod_u->dry ) {
        nod_u = nod_u->nex_u;
      }
      else {
        if ( c3y == nod_u->dir ) {
          DIR* red_u = c3_opendir(nod_u->pax_c);
          if ( 0 == red_u ) {
            u3_unod* nex_u = nod_u->nex_u;
            can = u3kb_weld(_unix_free_node(unx_u, nod_u), can);
            nod_u = nex_u;
          }
          else {
            closedir(red_u);
            nod_u = nod_u->nex_u;
          }
        }
        else {
          struct stat buf_u;
          c3_i  fid_i = c3_open(nod_u->pax_c, O_RDONLY, 0644);

          if ( (fid_i < 0) || (fstat(fid_i, &buf_u) < 0) ) {
            if ( ENOENT != errno ) {
              u3l_log("_unix_update_dir: error opening file %s: %s",
                      nod_u->pax_c, strerror(errno));
            }

            u3_unod* nex_u = nod_u->nex_u;
            can = u3kb_weld(_unix_free_node(unx_u, nod_u), can);
            nod_u = nex_u;
          }
          else {
            if ( close(fid_i) < 0 ) {
              u3l_log("_unix_update_dir: error closing file %s: %s",
                      nod_u->pax_c, strerror(errno));
            }

            nod_u = nod_u->nex_u;
          }
        }
      }
    }
  }

  // Check for new nodes

  DIR* rid_u = c3_opendir(dir_u->pax_c);
  if ( !rid_u ) {
    u3l_log("error opening directory %s: %s",
            dir_u->pax_c, strerror(errno));
    c3_assert(0);
  }

  while ( 1 ) {
    struct dirent  ent_u;
    struct dirent* out_u;
    c3_w err_w;


    if ( (err_w = u3_readdir_r(rid_u, &ent_u, &out_u)) != 0 ) {
      u3l_log("error loading directory %s: %s",
              dir_u->pax_c, strerror(err_w));
      c3_assert(0);
    }
    else if ( !out_u ) {
      break;
    }
    else if ( '.' == out_u->d_name[0] ) {
      continue;
    }
    else {
      c3_c* pax_c = _unix_down(dir_u->pax_c, out_u->d_name);

      struct stat buf_u;

      if ( 0 != stat(pax_c, &buf_u) ) {
        u3l_log("can't stat %s: %s", pax_c, strerror(errno));
        c3_free(pax_c);
        continue;
      }
      else {
        u3_unod* nod_u;
        for ( nod_u = dir_u->kid_u; nod_u; nod_u = nod_u->nex_u ) {
          if ( 0 == strcmp(pax_c, nod_u->pax_c) ) {
            if ( S_ISDIR(buf_u.st_mode) ) {
              if ( c3n == nod_u->dir ) {
                u3l_log("not a directory: %s", nod_u->pax_c);
                c3_assert(0);
              }
            }
            else {
              if ( c3y == nod_u->dir ) {
                u3l_log("not a file: %s", nod_u->pax_c);
                c3_assert(0);
              }
            }
            break;
          }
        }

        if ( !nod_u ) {
          if ( !S_ISDIR(buf_u.st_mode) ) {
            if (  !strchr(out_u->d_name,'.')
               || '~' == out_u->d_name[strlen(out_u->d_name) - 1]
               || !_unix_sane_ta(unx_u, _unix_string_to_knot(out_u->d_name)) )
            {
              c3_free(pax_c);
              continue;
            }

            u3_ufil* fil_u = c3_malloc(sizeof(u3_ufil));
            _unix_watch_file(unx_u, fil_u, dir_u, pax_c);
          }
          else {
            u3_udir* dis_u = c3_malloc(sizeof(u3_udir));
            _unix_watch_dir(dis_u, dir_u, pax_c);
            can = u3kb_weld(_unix_update_dir(unx_u, dis_u), can); // XXX unnecessary?
          }
        }
      }

      c3_free(pax_c);
    }
  }

  if ( closedir(rid_u) < 0 ) {
    u3l_log("error closing directory %s: %s",
            dir_u->pax_c, strerror(errno));
  }

  if ( !dir_u->kid_u ) {
    return u3kb_weld(_unix_free_node(unx_u, (u3_unod*) dir_u), can);
  }

  // get change list

  for ( nod_u = dir_u->kid_u; nod_u; nod_u = nod_u->nex_u ) {
    can = u3kb_weld(_unix_update_node(unx_u, nod_u), can);
  }

  return can;
}

/* _unix_update_node(): update node, producing list of changes
*/
static u3_noun
_unix_update_node(u3_unix* unx_u, u3_unod* nod_u)
{
  if ( c3y == nod_u->dir ) {
    return _unix_update_dir(unx_u, (void*)nod_u);
  }
  else {
    return _unix_update_file(unx_u, (void*)nod_u);
  }
}

/* _unix_update_mount(): update mount point
*/
static void
_unix_update_mount(u3_unix* unx_u, u3_umon* mon_u, u3_noun all)
{
  if ( c3n == mon_u->dir_u.dry ) {
    u3_noun  can = u3_nul;
    u3_unod* nod_u;
    for ( nod_u = mon_u->dir_u.kid_u; nod_u; nod_u = nod_u->nex_u ) {
      can = u3kb_weld(_unix_update_node(unx_u, nod_u), can);
    }

    {
      //  XX remove u3A->sen
      //
      u3_noun wir = u3nt(c3__sync,
                        u3dc("scot", c3__uv, unx_u->sev_l),
                        u3_nul);
      u3_noun cad = u3nq(c3__into, _unix_string_to_knot(mon_u->nam_c), all,
                         can);

      u3_auto_plan(&unx_u->car_u, u3_ovum_init(0, c3__c, wir, cad));
    }
  }
}

/* _unix_initial_update_file(): read file, but don't watch
**  XX deduplicate with _unix_update_file()
*/
static u3_noun
_unix_initial_update_file(c3_c* pax_c, c3_c* bas_c)
{
  struct stat buf_u;
  c3_i  fid_i = c3_open(pax_c, O_RDONLY, 0644);
  c3_ws len_ws, red_ws;
  c3_y* dat_y;

  if ( fid_i < 0 || fstat(fid_i, &buf_u) < 0 ) {
    if ( ENOENT == errno ) {
      return u3_nul;
    }
    else {
      u3l_log("error opening initial file %s: %s",
              pax_c, strerror(errno));
      return u3_nul;
    }
  }

  len_ws = buf_u.st_size;
  dat_y = c3_malloc(len_ws);

  red_ws = read(fid_i, dat_y, len_ws);

  if ( close(fid_i) < 0 ) {
    u3l_log("error closing initial file %s: %s",
            pax_c, strerror(errno));
  }

  if ( len_ws != red_ws ) {
    if ( red_ws < 0 ) {
      u3l_log("error reading initial file %s: %s",
              pax_c, strerror(errno));
    }
    else {
      u3l_log("wrong # of bytes read in initial file %s: %d %d",
              pax_c, len_ws, red_ws);
    }
    c3_free(dat_y);
    return u3_nul;
  }
  else {
    u3_noun pax = _unix_string_to_path_helper(pax_c
                   + strlen(bas_c)
                   + 1); /* XX slightly less VERY BAD than before*/
    u3_noun mim = u3nt(c3__text, u3i_string("plain"), u3_nul);
    u3_noun dat = u3nt(mim, len_ws, u3i_bytes(len_ws, dat_y));

    c3_free(dat_y);
    return u3nc(u3nt(pax, u3_nul, dat), u3_nul);
  }
}

/* _unix_initial_update_dir(): read directory, but don't watch
**  XX deduplicate with _unix_update_dir()
*/
static u3_noun
_unix_initial_update_dir(c3_c* pax_c, c3_c* bas_c)
{
  u3_noun can = u3_nul;

  DIR* rid_u = c3_opendir(pax_c);
  if ( !rid_u ) {
    u3l_log("error opening initial directory: %s: %s",
            pax_c, strerror(errno));
    return u3_nul;
  }

  while ( 1 ) {
    struct dirent  ent_u;
    struct dirent* out_u;
    c3_w err_w;

    if ( 0 != (err_w = u3_readdir_r(rid_u, &ent_u, &out_u)) ) {
      u3l_log("error loading initial directory %s: %s",
              pax_c, strerror(errno));

      c3_assert(0);
    }
    else if ( !out_u ) {
      break;
    }
    else if ( '.' == out_u->d_name[0] ) {
      continue;
    }
    else {
      c3_c* pox_c = _unix_down(pax_c, out_u->d_name);

      struct stat buf_u;

      if ( 0 != stat(pox_c, &buf_u) ) {
        u3l_log("initial can't stat %s: %s",
                pox_c, strerror(errno));
        c3_free(pox_c);
        continue;
      }
      else {
        if ( S_ISDIR(buf_u.st_mode) ) {
          can = u3kb_weld(_unix_initial_update_dir(pox_c, bas_c), can);
        }
        else {
          can = u3kb_weld(_unix_initial_update_file(pox_c, bas_c), can);
        }
        c3_free(pox_c);
      }
    }
  }

  if ( closedir(rid_u) < 0 ) {
    u3l_log("error closing initial directory %s: %s",
            pax_c, strerror(errno));
  }

  return can;
}

/* u3_unix_initial_into_card(): create initial filesystem sync card.
*/
u3_noun
u3_unix_initial_into_card(c3_c* arv_c)
{
  u3_noun can = _unix_initial_update_dir(arv_c, arv_c);

  return u3nc(u3nt(c3__c, c3__sync, u3_nul),
              u3nq(c3__into, u3_nul, c3y, can));
}

/* _unix_sync_file(): sync file to unix
*/
static void
_unix_sync_file(u3_unix* unx_u, u3_udir* par_u, u3_noun nam, u3_noun ext, u3_noun mim)
{
  c3_assert( par_u );
  c3_assert( c3y == par_u->dir );

  // form file path

  c3_c* nam_c = _unix_knot_to_string(nam);
  c3_c* ext_c = _unix_knot_to_string(ext);
  c3_w  par_w = strlen(par_u->pax_c);
  c3_w  nam_w = strlen(nam_c);
  c3_w  ext_w = strlen(ext_c);
  c3_c* pax_c = c3_malloc(par_w + 1 + nam_w + 1 + ext_w + 1);

  strcpy(pax_c, par_u->pax_c);
  pax_c[par_w] = '/';
  strcpy(pax_c + par_w + 1, nam_c);
  pax_c[par_w + 1 + nam_w] = '.';
  strcpy(pax_c + par_w + 1 + nam_w + 1, ext_c);
  pax_c[par_w + 1 + nam_w + 1 + ext_w] = '\0';

  c3_free(nam_c); c3_free(ext_c);
  u3z(nam); u3z(ext);

  // check whether we already know about this file

  u3_unod* nod_u;
  for ( nod_u = par_u->kid_u;
        ( nod_u &&
          ( c3y == nod_u->dir ||
            0 != strcmp(nod_u->pax_c, pax_c) ) );
        nod_u = nod_u->nex_u )
  { }

  // apply change

  if ( u3_nul == mim ) {
    if ( nod_u ) {
      u3z(_unix_free_node(unx_u, nod_u));
    }
  }
  else {

    if ( !nod_u ) {
      c3_w gum_w = _unix_write_file_hard(pax_c, u3k(u3t(mim)));
      u3_ufil* fil_u = c3_malloc(sizeof(u3_ufil));
      _unix_watch_file(unx_u, fil_u, par_u, pax_c);
      fil_u->gum_w = gum_w;
      goto _unix_sync_file_out;
    }
    else {
      _unix_write_file_soft((u3_ufil*) nod_u, u3k(u3t(mim)));
    }
  }

  c3_free(pax_c);

_unix_sync_file_out:
  u3z(mim);
}

/* _unix_sync_change(): sync single change to unix
*/
static void
_unix_sync_change(u3_unix* unx_u, u3_udir* dir_u, u3_noun pax, u3_noun mim)
{
  c3_assert( c3y == dir_u->dir );

  if ( c3n == u3du(pax) ) {
    if ( u3_nul == pax ) {
      u3l_log("can't sync out file as top-level, strange");
    }
    else {
      u3l_log("sync out: bad path");
    }
    u3z(pax); u3z(mim);
    return;
  }
  else if ( c3n == u3du(u3t(pax)) ) {
    u3l_log("can't sync out file as top-level, strangely");
    u3z(pax); u3z(mim);
  }
  else {
    u3_noun i_pax = u3h(pax);
    u3_noun t_pax = u3t(pax);
    u3_noun it_pax = u3h(t_pax);
    u3_noun tt_pax = u3t(t_pax);

    if ( u3_nul == tt_pax ) {
      _unix_sync_file(unx_u, dir_u, u3k(i_pax), u3k(it_pax), mim);
    }
    else {
      c3_c* nam_c = _unix_knot_to_string(i_pax);
      c3_w pax_w = strlen(dir_u->pax_c);
      u3_unod* nod_u;

      for ( nod_u = dir_u->kid_u;
            ( nod_u &&
              ( c3n == nod_u->dir ||
                0 != strcmp(nod_u->pax_c + pax_w + 1, nam_c) ) );
            nod_u = nod_u->nex_u )
      { }

      if ( !nod_u ) {
        nod_u = c3_malloc(sizeof(u3_udir));
        _unix_create_dir((u3_udir*) nod_u, dir_u, u3k(i_pax));
      }

      if ( c3n == nod_u->dir ) {
        u3l_log("weird, we got a file when we weren't expecting to");
        c3_assert(0);
      }

      _unix_sync_change(unx_u, (u3_udir*) nod_u, u3k(t_pax), mim);
    }
  }
  u3z(pax);
}

/* _unix_sync_ergo(): sync list of changes to unix
*/
static void
_unix_sync_ergo(u3_unix* unx_u, u3_umon* mon_u, u3_noun can)
{
  u3_noun nac = can;
  u3_noun nam = _unix_string_to_knot(mon_u->nam_c);

  while ( u3_nul != nac) {
    _unix_sync_change(unx_u, &mon_u->dir_u,
                      u3nc(u3k(nam), u3k(u3h(u3h(nac)))),
                      u3k(u3t(u3h(nac))));
    nac = u3t(nac);
  }

  u3z(nam);
  u3z(can);
}

/* u3_unix_ef_dirk(): commit mount point
*/
void
u3_unix_ef_dirk(u3_unix* unx_u, u3_noun mon)
{
  _unix_commit_mount_point(unx_u, mon);
}

/* u3_unix_ef_ergo(): update filesystem from urbit
*/
void
u3_unix_ef_ergo(u3_unix* unx_u, u3_noun mon, u3_noun can)
{
  u3_umon* mon_u = _unix_get_mount_point(unx_u, mon);

  _unix_sync_ergo(unx_u, mon_u, can);
}

/* u3_unix_ef_ogre(): delete mount point
*/
void
u3_unix_ef_ogre(u3_unix* unx_u, u3_noun mon)
{
  _unix_delete_mount_point(unx_u, mon);
}

/* u3_unix_ef_hill(): enumerate mount points
*/
void
u3_unix_ef_hill(u3_unix* unx_u, u3_noun hil)
{
  u3_noun mon;

  for ( mon = hil; c3y == u3du(mon); mon = u3t(mon) ) {
    u3_umon* mon_u = _unix_get_mount_point(unx_u, u3k(u3h(mon)));
    _unix_scan_mount_point(unx_u, mon_u);
  }

  unx_u->car_u.liv_o = c3y;

  u3z(hil);
}

/* u3_unix_ef_look(): update the root of a specific mount point.
*/
void
u3_unix_ef_look(u3_unix* unx_u, u3_noun mon, u3_noun all)
{
  if ( c3y == unx_u->dyr ) {
    c3_c* nam_c = _unix_knot_to_string(mon);

    unx_u->dyr = c3n;
    u3_umon* mon_u = unx_u->mon_u;
    while ( mon_u && 0 != strcmp(nam_c, mon_u->nam_c) ) {
      mon_u = mon_u->nex_u;
    }
    c3_free(nam_c);
    if ( mon_u ) {
      _unix_update_mount(unx_u, mon_u, all);
    }
  }
  u3z(mon);
}

/* _unix_io_talk(): start listening for fs events.
*/
static void
_unix_io_talk(u3_auto* car_u)
{
  //  XX review wire
  //
  u3_noun wir = u3nc(c3__boat, u3_nul);
  u3_noun cad = u3nc(c3__boat, u3_nul);

  u3_auto_plan(car_u, u3_ovum_init(0, c3__c, wir, cad));
}

/* _unix_io_kick(): apply effects.
*/
static c3_o
_unix_io_kick(u3_auto* car_u, u3_noun wir, u3_noun cad)
{
  u3_unix* unx_u = (u3_unix*)car_u;

  u3_noun tag, dat, i_wir;
  c3_o ret_o;

  if (  (c3n == u3r_cell(wir, &i_wir, 0))
     || (c3n == u3r_cell(cad, &tag, &dat))
     || (  (c3__clay != i_wir)
        && (c3__boat != i_wir)
        && (c3__sync != i_wir) )  )
  {
    ret_o = c3n;
  }
  else {
    switch ( tag ) {
      default: {
        ret_o = c3n;
      } break;

      case c3__dirk: {
        u3_unix_ef_dirk(unx_u, u3k(dat));
        ret_o = c3y;
      } break;

      case c3__ergo: {
        u3_noun mon = u3k(u3h(dat));
        u3_noun can = u3k(u3t(dat));
        u3_unix_ef_ergo(unx_u, mon, can);

        ret_o = c3y;
      } break;

      case c3__ogre: {
        u3_unix_ef_ogre(unx_u, u3k(dat));
        ret_o = c3y;
      } break;

      case c3__hill: {
        u3_unix_ef_hill(unx_u, u3k(dat));
        ret_o = c3y;
      } break;
    }
  }

  u3z(wir); u3z(cad);
  return ret_o;
}

/* _unix_io_exit(): terminate unix I/O.
*/
static void
_unix_io_exit(u3_auto* car_u)
{
  u3_unix* unx_u = (u3_unix*)car_u;

  u3z(unx_u->sat);
  c3_free(unx_u->pax_c);
  c3_free(unx_u);
}

/* u3_unix_io_init(): initialize unix sync.
*/
u3_auto*
u3_unix_io_init(u3_pier* pir_u)
{
  u3_unix* unx_u = c3_calloc(sizeof(*unx_u));
  unx_u->mon_u = 0;
  unx_u->pax_c = strdup(pir_u->pax_c);
  unx_u->alm = c3n;
  unx_u->dyr = c3n;
  unx_u->sat = u3do("sane", c3__ta);

  u3_auto* car_u = &unx_u->car_u;
  car_u->nam_m = c3__unix;
  car_u->liv_o = c3n;
  car_u->io.talk_f = _unix_io_talk;
  car_u->io.kick_f = _unix_io_kick;
  car_u->io.exit_f = _unix_io_exit;
  //  XX wat do
  //
  // car_u->ev.bail_f = ...l;

  {
    u3_noun now;
    struct timeval tim_u;
    gettimeofday(&tim_u, 0);

    now = u3_time_in_tv(&tim_u);
    unx_u->sev_l = u3r_mug(now);
    u3z(now);
  }

  return car_u;
}
