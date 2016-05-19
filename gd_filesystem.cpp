/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

/*
 * 
 * g++ -Wall -l:libfuse.so.2 -o gd gd_filesystem.cpp
 * g++ -Wall -l:libfuse.so.2 -o gd gd_filesystem.cpp gd_cache.cpp
 * 
 */

#define FUSE_USE_VERSION 30

//#include <config.h>

#include "fuse_lowlevel.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <list>

#include "gd_cache.h"

//static const char *hello_str = "Hello World!\n";
//static const char *hello_name = "hello";

// TODO: - string passings recheck

/*static const char* entry_nev_pont = ".";
static const char* entry_nev_ppont = "..";
static const char* entry_nev_szia = "szia";
static int statino = 0;*/

static void log( const char* format, ... ) {
	return;
	va_list args;
	fprintf( stdout, "Log:fs# " );
	va_start( args, format );
	vfprintf( stdout, format, args );
	va_end( args );
	fprintf( stdout, "\n" );
	fflush(stdout);
}



static void gd_init(void* userdata, struct fuse_conn_info *conn){
	log("init");
	gd_cache::init("backup.dat");	// create a root folder, or load backup if exist
}
static void gd_destroy(void* userdata){
	log("destroy");
	gd_cache::save("backup.dat");
}
static void gd_lookup(fuse_req_t req, fuse_ino_t parent, const char *name){
	log("lookup: %ld, %s",parent,name);
	
	entry* lparent = gd_cache::getentry(parent);
	if(lparent == NULL || !lparent->isfolder() ){
		fuse_reply_err(req, ENOENT);
		return;
	}
	entry* lentry = ((folder*) lparent)->getchild(name);
	if(lentry == NULL){
		fuse_reply_err(req, ENOENT);
		return;
	}
	
	struct fuse_entry_param e;
	
	memset(&e, 0, sizeof(e));
	
	lentry->get_attr(e.attr);
	
	e.ino = e.attr.st_ino;
	e.attr_timeout = 1.0;
	e.entry_timeout = 1.0;
	
	fuse_reply_entry(req, &e);
}
static void gd_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup){
	log("forget: %lli, %lli",ino,nlookup);
	fuse_reply_none(req);
}
static void gd_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
	log("gd_getattr: %ld",ino);
	
	entry* lentry = gd_cache::getentry(ino);
	if(lentry == NULL){
		fuse_reply_err(req, ENOENT);
		return;
	}
	struct stat stbuf;
	
	lentry->get_attr(stbuf);
	
	fuse_reply_attr(req, &stbuf, 1.0);	// last parameter is the validity in seconds (double)
}
static void gd_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi){
	log("setattr: %ld, %o",ino,to_set);
	// to_set is a mask to attr
	
	entry* lentry = gd_cache::getentry(ino);
	if(lentry == NULL){
		fuse_reply_err(req, ENOENT);
		return;
	}
	
	lentry->set_attr(*attr, to_set);
	
	struct stat stbuf;
	
	lentry->get_attr(stbuf);
	
	fuse_reply_attr(req, &stbuf, 1.0);	// last parameter is the validity in seconds (double)
}
/*static void gd_readlink(fuse_req_t req, fuse_ino_t ino){		not supported yet
	log("readlink: %ld",ino);
	fuse_reply_readlink(req, buf);
}*/
static void gd_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode){
	log("mkdir: %ld, %s, %o",parent,name,mode);
	
	entry* lparent = gd_cache::getentry(parent);
	if(lparent == NULL || !lparent->isfolder() ){
		fuse_reply_err(req, ENOENT);
		return;
	}
	
	folder* newchild = new folder(name);
	
	struct fuse_entry_param e;
	memset(&e, 0, sizeof(e));
	e.attr.st_mode = mode;
	e.attr.st_ctime = time(NULL);
	
	newchild->set_attr( e.attr, FUSE_SET_ATTR_MODE | FUSE_SET_ATTR_ATIME_NOW | FUSE_SET_ATTR_MTIME_NOW | FUSE_SET_ATTR_CTIME);
	
	((folder*) lparent)->addchild(newchild);
	lparent->modify();
	
	newchild->get_attr( e.attr);
	e.ino = e.attr.st_ino;
	e.attr_timeout = 1.0;
	e.entry_timeout = 1.0;
	
	fuse_reply_entry( req, &e);
}
static void gd_unlink(fuse_req_t req, fuse_ino_t parent, const char *name){	// same as rmdir
	log("unlink: %ld, %s",parent,name);
	
	entry* lparent = gd_cache::getentry(parent);
	if(lparent == NULL || !lparent->isfolder() ){
		fuse_reply_err(req, ENOENT);
		return;
	}
	entry* lentry = ((folder*) lparent)->getchild(name);
	if(lentry == NULL){
		fuse_reply_err(req, ENOENT);
		return;
	}
	
	gd_cache::removeentry( lentry );	// this remove from the parent, and delete
	lparent->modify();
	
	fuse_reply_err(req, 0);	// that 0 means success!
}
static void gd_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name){	// same as unlink
	log("rmdir: %ld, %s",parent,name);
	
	entry* lparent = gd_cache::getentry(parent);
	if(lparent == NULL || !lparent->isfolder() ){
		fuse_reply_err(req, ENOENT);
		return;
	}
	entry* lentry = ((folder*) lparent)->getchild(name);
	if(lentry == NULL || !lentry->isfolder() ){
		fuse_reply_err(req, ENOENT);
		return;
	}
	
	gd_cache::removeentry( lentry );	// this remove from the parent, delete recursively the content (not neceserry, the os take care before), and delete the folder
	lparent->modify();
	
	fuse_reply_err(req, 0);	// that 0 means success!
}
static void gd_rename(fuse_req_t req, fuse_ino_t oldparent, const char *oldname, fuse_ino_t newparent, const char *newname, unsigned int flags){
	log("rename: %ld, %s, %ld, %s",oldparent,oldname,newparent,newname);
	
	entry* loldparent = gd_cache::getentry(oldparent);
	if(loldparent == NULL || !loldparent->isfolder() ){
		fuse_reply_err(req, ENOENT);
		return;
	}
	entry* lentry = ((folder*) loldparent)->getchild(oldname);
	if(lentry == NULL){
		fuse_reply_err(req, ENOENT);
		return;
	}
	
	if(oldparent != newparent){
		entry* lnewparent = gd_cache::getentry(newparent);
		if(lnewparent == NULL || !lnewparent->isfolder() ){
			fuse_reply_err(req, ENOENT);
			return;
		}
		lentry->set_parent((folder*)lnewparent);	// set the entry parent: remove from the old parent, add to the new
		lnewparent->modify();
	}
	
	loldparent->modify();
	
	lentry->rename(newname);
	
	fuse_reply_err(req, 0);	// that 0 means success!
}
static void gd_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
	log("open: %ld",ino);
	//if (ino != 2)
	//	fuse_reply_err(req, EISDIR);
	//else if ((fi->flags & 3) != O_RDONLY)
	//	fuse_reply_err(req, EACCES);
	//else
		fuse_reply_open(req, fi);
}
static void gd_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi){
	log("read: %ld, %ld, %ld",ino,size,off);
	
	entry* lentry = gd_cache::getentry(ino);
	if(lentry == NULL || lentry->isfolder() ){
		fuse_reply_err(req, ENOENT);
		return;
	}
	
	const char* buf;
	size_t len = ((file*) lentry)->read( buf, size, off);
	
	fuse_reply_buf(req, buf, len);
}
static void gd_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi){
	log("write: %ld, buf, %u, %d",ino,size,off);
	
	entry* lentry = gd_cache::getentry(ino);
	if(lentry == NULL || lentry->isfolder() ){
		fuse_reply_err(req, ENOENT);
		return;
	}
	
	fuse_reply_write(req, ((file*) lentry)->write( buf, size, off) );	// return with the success writed byte size
}
static void gd_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
	log("flush: %ld",ino);
	fuse_reply_err(req, 0);	// that 0 means success!
}
static void gd_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
	log("release: %ld",ino);
	fuse_reply_err(req, 0);	// that 0 means success!
}
static void gd_fsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi){
	log("fsync: %ld, %d",ino,datasync);
	fuse_reply_err(req, 0);	// that 0 means success!
}
static void gd_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
	log("opendir: %ld",ino);
	fuse_reply_open(req, fi);
	//fuse_reply_err(req, error);// ENOMEM, errno
}
static void gd_readdir(fuse_req_t req, fuse_ino_t ino, size_t maxsize, off_t offset, struct fuse_file_info *fi){
	// . and .. are necesserry to add?
	log("readdir: %ld, %ld, %ld",ino,maxsize,offset);
	
	entry* lparent = gd_cache::getentry(ino);
	if(lparent == NULL || !lparent->isfolder() ){
		fuse_reply_err(req, ENOTDIR);
		return;
	}
	
	char* buf = (char*) calloc(maxsize,1);
	size_t bufsize = 0;
	size_t entry_size;
	struct stat stbuf;
	const char* name;
	
	entry* tchild;
	int childcount = ((folder*) lparent)->childssize();
	if(offset >= childcount){
		log("\tvege");
		fuse_reply_buf(req, NULL, 0);
		free(buf);
		return;
	}
	std::list<entry* >::const_iterator diriter = ((folder*) lparent)->getchilditer();
	for(int i=0;i<offset;i++){
		// seek to the offset, is there a better way? (i tried the 'diriter + offset', but it didn't compile for first)
		diriter++;
	}
	for(int i=offset; i < childcount ; i++, diriter++ ){
		tchild = ((folder*) lparent)->getchild( diriter );	// diriter passed as reference
		if(tchild == NULL){
			log("HIBA: readdir: tchild == NULL: %d",i);
			break;
		}
		name = tchild->get_name().c_str();
		//ino = tchild->get_ino();
		entry_size = fuse_add_direntry(req, NULL, 0, name, NULL, 0);//fuse_dirent_size(strlen(name));
		if( bufsize +  entry_size > maxsize){
			break;
		}
		tchild->get_attr(stbuf);
		bufsize += fuse_add_direntry(req, buf+bufsize, maxsize-bufsize, name, &stbuf, i+1);	// last parameter (i) is the offset
	}
	
	log("\treturn: %d",fuse_reply_buf(req, buf, bufsize));
	free(buf);
}
static void gd_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
	log("releasedir: %ld",ino);
	fuse_reply_err(req, 0);	// that 0 means success!
}

static void gd_fsyncdir(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi){
	log("fsyncdir: %ld, %d",ino,datasync);
	fuse_reply_err(req, 0);	// that 0 means success!
}
static void gd_statfs(fuse_req_t req, fuse_ino_t ino){
	log("statfs: %ld",ino);
	struct statvfs stbuf;
	memset(&stbuf, 0, sizeof(stbuf));
	
	stbuf.f_bsize = 1024;				/* Filesystem block size */
	stbuf.f_frsize = 1024;				/* Fragment size */
	stbuf.f_blocks = 100;//*f_frsize	/* Size of fs in f_frsize units */
	stbuf.f_bfree = 99;//*f_frsize		/* Number of free blocks */
	stbuf.f_bavail = 99;//*f_frsize		/* Number of free blocks for unprivileged users */
	stbuf.f_files = 128;				/* Number of inodes */
	stbuf.f_ffree = 556;				/* Number of free inodes */
	stbuf.f_favail = 212;				/* Number of free inodes for unprivileged users */
	//stbuf.f_fsid = ;					/* Filesystem ID */
	//stbuf.f_flag = ;					/* Mount flags */	// ST_RDONLY, ST_NOSUID
	stbuf.f_namemax = 255;				/* Maximum filename length */
	
	fuse_reply_statfs( req, &stbuf);
}
static void gd_access(fuse_req_t req, fuse_ino_t ino, int mask){
	log("access: %ld, %o",ino,mask);	// mask := F_OK (exist), R_OK, W_OK, X_OK
	
	entry* lentry = gd_cache::getentry(ino);
	if(lentry == NULL){
		fuse_reply_err(req, ENOENT);
		return;
	}
	
	fuse_reply_err(req, 0);	// that 0 means success!
}
static void gd_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi){
	log("create: %ld, %s, %o",parent,name,mode);
	
	entry* lparent = gd_cache::getentry(parent);
	if(lparent == NULL || !lparent->isfolder() ){
		fuse_reply_err(req, ENOENT);
		return;
	}
	
	file* newchild = new file(name);
	
	struct fuse_entry_param e;
	memset(&e, 0, sizeof(e));
	e.attr.st_mode = mode;
	e.attr.st_ctime = time(NULL);
	
	newchild->set_attr( e.attr, FUSE_SET_ATTR_MODE | FUSE_SET_ATTR_ATIME_NOW | FUSE_SET_ATTR_MTIME_NOW | FUSE_SET_ATTR_CTIME);
	
	((folder*) lparent)->addchild(newchild);
	lparent->modify();
	
	newchild->get_attr( e.attr);
	e.ino = e.attr.st_ino;
	e.attr_timeout = 1.0;
	e.entry_timeout = 1.0;
	
	fuse_reply_create( req, &e, fi);
}
static void gd_forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets){
	log("forget: %u",count);
	fuse_reply_none(req);
}
static void gd_fallocate(fuse_req_t req, fuse_ino_t ino, int mode, off_t offset, off_t length, struct fuse_file_info *fi){
	log("fallocate: %ld, %o, %d, %d",ino,mode,offset,length);
	
	entry* lentry = gd_cache::getentry(ino);
	if(lentry == NULL || lentry->isfolder() ){
		fuse_reply_err(req, ENOENT);
		return;
	}
	
	((file*) lentry)->allocate(offset, length);
	
	fuse_reply_err(req, 0);	// that 0 means success!
}
static struct fuse_lowlevel_ops gd_op = {		// the order is very important!!!
	/*init*/			gd_init,
	/*destroy*/			gd_destroy,
	/*lookup*/			gd_lookup,
	/*forget*/			gd_forget,
	/*getattr*/			gd_getattr,
	/*setattr*/			gd_setattr,
	/*readlink*/		NULL,//gd_readlink,
	/*mknod*/			NULL,//gd_mknod,
	/*mkdir*/			gd_mkdir,
	/*unlink*/			gd_unlink,
	/*rmdir*/			gd_rmdir,
	/*symlink*/			NULL,//gd_symlink,
	/*rename*/			gd_rename,
	/*link*/			NULL,//gd_link,
	/*open*/			gd_open,
	/*read*/			gd_read,
	/*write*/			gd_write,
	/*flush*/			gd_flush,
	/*release*/			gd_release,
	/*fsync*/			gd_fsync,
	/*opendir*/			gd_opendir,
	/*readdir*/			gd_readdir,
	/*releasedir*/		gd_releasedir,
	/*fsyncdir*/		gd_fsyncdir,
	/*statfs*/			gd_statfs,
	/*setxattr*/		NULL,//gd_setxattr,
	/*getxattr*/		NULL,//gd_getxattr,
	/*listxattr*/		NULL,//gd_listxattr,
	/*removexattr*/		NULL,//gd_removexattr,
	/*access*/			gd_access,
	/*create*/			gd_create,
	/*getlk*/			NULL,//gd_getlk,	// POSIX file lock
	/*setlk*/			NULL,//gd_setlk,
	/*bmap*/			NULL,//gd_bmap,
	/*ioctl*/			NULL,//gd_ioctl,
	/*poll*/			NULL,//gd_poll,
	/*write_buf*/		NULL,//gd_write_buf,		TODO
	/*retrieve_reply*/	NULL,//gd_retrieve_reply,
	/*forget_multi*/	gd_forget_multi,
	/*flock*/			NULL,//gd_flock,
	/*fallocate*/		gd_fallocate,
	/*readdirplus*/		NULL,//gd_readdirplus				TODO
};

int main(int argc, char *argv[]){
	char* cargc[2];
		  cargc[0] = (char*) "./gd";
		  cargc[1] = (char*) "m";
		  //cargc[2] = (char*) "-d";
	struct fuse_args args = { sizeof(cargc)/sizeof(cargc[0]) , cargc, 0};//FUSE_ARGS_INIT(argc, argv);
	struct fuse_chan *ch;
	//const char *mountpoint = "m";
	char *mountpoint;
	int err = -1;
	
	/*log("argc: %d",args.argc);
	for(int i=0;i<args.argc;i++){
		log("\t%d: %s",i,args.argv[i]);
	}*/
	
	if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
		(ch = fuse_mount(mountpoint, &args)) != NULL) {
	//if ((ch = fuse_mount(mountpoint, &args)) != NULL) {
		struct fuse_session *se;
		
		log("mountő: %s",mountpoint);
		
		se = fuse_lowlevel_new(&args, &gd_op,
				       sizeof(gd_op), NULL);
		if (se != NULL) {
			if (fuse_set_signal_handlers(se) != -1) {
				fuse_session_add_chan(se, ch);
				
				/* Block until ctrl+c or fusermount -u */
				err = fuse_session_loop(se);
				
				fuse_remove_signal_handlers(se);
				fuse_session_remove_chan(ch);
			}
			fuse_session_destroy(se);
		}
		log("unmount");
		fuse_unmount(mountpoint, ch);
	}
	fuse_opt_free_args(&args);
	
	return err ? 1 : 0;
}