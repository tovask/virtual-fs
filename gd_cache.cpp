//http://www.cplusplus.com/reference/list/list/
//http://www.tutorialspoint.com/cplusplus/cpp_stl_tutorial.htm
//http://www.eet.bme.hu/~czirkos/mutat.php?mit=orokles_perzisztencia.cpp

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>	// for log
#include <iostream>
#include <fstream>
#include <string.h>
#include <list>
#include <exception>

#define FUSE_USE_VERSION 30		// ehhez majd egy kozos header kene
#include "fuse_lowlevel.h"

#include "gd_cache.h"

// class gd_cache is all static
// pointers to reference

// TODO:
//		-stat uid,gid,times
//		-getnextavailableino and addnewino better be in one function
//		-inonext, root_ino
//		-clearify the string pass to function
//		-organize error codes
//		-handle mem alloc error

using namespace std;

// logging functions:
static bool logenable = true;
static void log( const char* format, ... ) {
	if(!logenable)return;
	va_list args;
	fprintf( stdout, "Log:cache# " );
	va_start( args, format );
	vfprintf( stdout, format, args );
	va_end( args );
	fprintf( stdout, "\n" );
	fflush(stdout);
}
static void log( string szov){
	if(!logenable)return;
	cout << "Log:cache# " << szov << endl;
	fflush(stdout);
}

// exception classes:
class myerror : public exception{
	string details;
public:
	const char* what () const throw (){
		return details.c_str();
	}
	myerror(string d):details(d){
		
	}
	virtual ~myerror()throw(){
		log("myexception destructor: "+details);
	}
};
class error_alreadyexist: public myerror{
	public: error_alreadyexist(string d):myerror(d){}
};
class error_iamtheroot: public myerror{
	public: error_iamtheroot(string d):myerror(d){}
};

// entry class:
entry::entry(string n, mode_t m, bool f):ino(gd_cache::getnextavailableino()), name(n), size(0), ctime(0), atime(0), mtime(0), mode(m), gid(0), uid(0), nlink(f ? 2 : 1), parent(0), is_folder(f){
	cout << "entry constructor: " << n << endl;
	gd_cache::addnewentry(this);
}
bool entry::isfolder(){
	return is_folder;
}
void entry::get_attr(struct stat & stbuf){
	memset(&stbuf, 0, sizeof(stbuf));
	stbuf.st_ino = ino;
	stbuf.st_size = size;
	stbuf.st_ctime = ctime;
	stbuf.st_atime = atime;
	stbuf.st_mtime = mtime;
	stbuf.st_mode = mode;
	stbuf.st_gid = gid;
	stbuf.st_uid = uid;
	stbuf.st_nlink = nlink;
}
void entry::set_attr(struct stat & attr, int to_set){
	logenable = false;
	log("set_attr");
	
	if( to_set & FUSE_SET_ATTR_MODE ){
		log("		FUSE_SET_ATTR_MODE:	%o",attr.st_mode);
		log("				before:	%o",mode);
		mode &= 0770000;		// this is in octal, not sure about the value
		mode |= attr.st_mode;
		log("				after :	%o",mode);
	}
	if( to_set & FUSE_SET_ATTR_UID ){
		log("		FUSE_SET_ATTR_UID");
		uid = attr.st_uid;
	}
	if( to_set & FUSE_SET_ATTR_GID ){
		log("		FUSE_SET_ATTR_GID");
		gid = attr.st_gid;
	}
	if( to_set & FUSE_SET_ATTR_SIZE ){
		log("		FUSE_SET_ATTR_SIZE: %d",attr.st_size);
		//size = attr.st_size;			// shouldn't we do smthing here?	may not.
		if( this->isfolder() ){
			size = attr.st_size;
		}else{									// not necesserry to allocate, ex: truncate set size, but not allocate it (but we do, to make the read easier)
			((file*) this)->allocate(0,attr.st_size);
		}
	}
	if( to_set & FUSE_SET_ATTR_ATIME ){
		log("		FUSE_SET_ATTR_ATIME");
		atime = attr.st_atime;
	}
	if( to_set & FUSE_SET_ATTR_MTIME ){
		log("		FUSE_SET_ATTR_MTIME");
		mtime = attr.st_mtime;
	}
	if( to_set & FUSE_SET_ATTR_ATIME_NOW ){
		log("		FUSE_SET_ATTR_ATIME_NOW");
		atime = time(NULL);
	}
	if( to_set & FUSE_SET_ATTR_MTIME_NOW ){
		log("		FUSE_SET_ATTR_MTIME_NOW");
		mtime = time(NULL);
	}
	if( to_set & FUSE_SET_ATTR_CTIME ){
		log("		FUSE_SET_ATTR_CTIME");
		ctime = attr.st_ctime;
	}
	logenable = true;
}
const fuse_ino_t& entry::get_ino(){
	return ino;
}
const string& entry::get_name(){
	return name;
}
void entry::rename(const string & newname){
	if(parent == 0 || parent->getchild(newname) != NULL){
		throw new error_alreadyexist(newname + " already exist (, or parent == 0)");
	}
	name = newname;
}
void entry::set_parent(folder* newparent){
	if( parent == 0){	// this is the root
		return;
	}
	parent->removechild(this);
	newparent->addchild(this);
	parent = newparent;
}
void entry::modify(){	// need more thinking, at least the name isn't good
	struct stat nop;
	this->set_attr( nop, FUSE_SET_ATTR_ATIME_NOW | FUSE_SET_ATTR_MTIME_NOW);
	if(this->parent != 0){
		this->parent->modify();
	}
}
void entry::onremove(){
	if( parent == 0 ){	// this should be the root, so what to do
		throw new error_iamtheroot("Cannot delete the root!");
	}
	if( isfolder() && ((folder*) this)->childssize() > 0){		// remove the childs (,as i saw it's not needed, already done before)
		list<entry* >::const_iterator diriter = ((folder*) this)->getchilditer();
		entry* tchild;
		while( ( tchild = ((folder*) this)->getchild( diriter ) ) != NULL ){
			gd_cache::removeentry( tchild );
			diriter++;
		}
	}
	
	parent->removechild( this );	// remove from parent
	
	delete this;	// suicide
}
ostream& operator<<(ostream& os, const entry& out){
	os << out.is_folder << " ";
	os << out.name << " ";
	os << out.size << " ";
	if(out.is_folder){
		os << ((folder*) &out)->childssize() << " ";
		list<entry* >::const_iterator diriter = ((folder*) &out)->getchilditer();
		entry* tchild;
		while( ( tchild = ((folder*) &out)->getchild( diriter ) ) != NULL ){
			os << *tchild;
			diriter++;
		}
	}else{	// if file
		const char* buf;
		off_t offset = 0;
		size_t cursize;
		cursize = ((file*)&out)->read(buf, 256, offset);
		while(cursize > 0){
			os.write( buf, cursize );
			offset += cursize;
			cursize = ((file*)&out)->read(buf, 256, offset);
		}
	}
	return os;
}
istream& operator>>(istream& is, entry& in){
	is >> in.name;
	is >> in.size;
	if(in.is_folder){
		int childssize;
		string inname;
		bool inis_folder;
		entry* inentry;
		
		is >> childssize;
		for(int i=0; i<childssize; i++){
			is >> inis_folder;
			if(inis_folder){
				inentry = new folder("");
			}else{
				inentry = new file("");
			}
			is >> *inentry;
			((folder*) &in)->addchild(inentry);
		}
	}else{	// if file
		char* buf = new char[256];
		off_t offset = 0;
		size_t remain = in.size;
		size_t cursize;
		is.read( buf, 1 ); // read the separate space
		while(remain > 0){
			cursize = 256<remain ? 256 : remain;
			is.read( buf, cursize );
			((file*)&in)->write(buf, cursize , offset);
			offset += cursize;
			remain -= cursize;
		}
		delete buf;
	}
	return is;
}


// folder class:
folder::folder(string n):entry(n,S_IFDIR | 0755,true){
	log("folder constructor: "+n);
}
int folder::childssize(){
	return childs.size();
}
void folder::addchild(entry* newentry){
	childs.push_back(newentry);
}
list<entry* >::const_iterator folder::getchilditer(){				// this is usually called on folder listing (readdir)
	return childs.begin();
}
entry* folder::getchild(list<entry* >::const_iterator & diriter){	// this is usually called on folder listing (readdir)
	if( diriter == childs.end()){		// how to do it more safely, ex: what if (diriter = childs.end() + 5) ???
		return NULL;					//  interesting info: after list.end() (which is invalid), the increase operator will set the iterator to list.begin()
	}
	return (*diriter);
}
entry* folder::getchild(const string& name){
	for(list<entry* >::const_iterator list_iter = childs.begin(); list_iter != childs.end(); list_iter++){
		if( (*list_iter)->get_name() == name ){
			return (*list_iter);
		}
	}
	return NULL;
}
void folder::removechild(entry* toremove){
	childs.remove( toremove );
}
folder::~folder(){
	log("folder destructor: " + name);
}

// file class:
file::file(string n):entry(n,S_IFREG | 0644,false),data(0),alldata(0){
	log("file constructor: "+n);
}
const char* file::get_data(){	// for saving , operator>>
	return data;
}
size_t file::read(const char* & buf, size_t size, off_t off){
	buf = data + off;
	return ((unsigned)off > this->size) ? 0 :  ( (off+size > this->size) ? this->size - off : size );	// make it prettier, please!
}
size_t file::write(const char* buf, size_t size, off_t off){
	if( data == 0 || alldata < off+size ){			// same as in allocate, we better have a set_minsize function which do this (and the allocate)
		data = (char*) realloc( data, (off+size)*sizeof(char) );
		alldata = off+size;
	}
	if( this->size < off+size ){	// had allocated before, but size was less, and now append
		this->size = off+size;
	}
	memcpy( data+off, buf, size);
	return size;
}
void file::allocate(off_t offset, off_t length){
	log("allocate: %d, %d",offset+length,this->size);
	if( (unsigned)(offset+length) > alldata ){
		data = (char*) realloc( data, (offset+length)*sizeof(char) );
		alldata = offset+length;
	}
	this->size = offset+length;
}
file::~file(){
	log("file destructor: " + name);
	free(data);
}

// gd_cache class:
list<entry *> gd_cache::all;
fuse_ino_t gd_cache::inonext = FUSE_ROOT_ID;

void gd_cache::init(const string& filename){
	inonext = FUSE_ROOT_ID;		// so give this to the next entry
	entry* inentry = new folder("root");			//  which will be this (at least, i hope so (,with a bit worry))
	ifstream infile (filename.c_str());
	if(infile.good()){
		bool inis_folder;
		infile >> inis_folder;	// we hope it is
		infile >> *inentry;
	}
	infile.close();
}
fuse_ino_t gd_cache::getnextavailableino(){		// optimalize with forget
	inonext++;			//the next
	return inonext-1;	//the actual
}
void gd_cache::addnewentry(entry* newino){
	logenable = false;
	log("addnewentry: %ld",newino->get_ino());
	logenable = true;
	all.push_back(newino);
}
entry* gd_cache::getentry(fuse_ino_t ino){
	for(list<entry* >::const_iterator list_iter = all.begin(); list_iter != all.end(); list_iter++){
		if( (*list_iter)->get_ino() == ino ){
			return (*list_iter);
		}
	}
	return NULL;
}
void gd_cache::removeentry(entry* toremove){		// should do it more safely, some if and try-catch needed
	try{
		toremove->onremove();
	}catch(error_iamtheroot* e){
		//TODO
		return;
	}
	all.remove( toremove );						// remove from ino list
}
void gd_cache::save(const string& filename){
	ofstream outfile;
	outfile.open(filename.c_str(), ios::out|ios::binary);
	outfile << *getentry(FUSE_ROOT_ID);
	outfile.close();
}

// TODO:
//  - fájlba