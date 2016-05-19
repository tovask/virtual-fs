
#ifndef _gd_cache_h_
#define _gd_cache_h_

// may we should include smthing for fuse, stat, string, list (now the outler cpp do this)

using namespace std;

class folder;
class gd_cache;

class entry{
protected:
	fuse_ino_t ino;
	string name;
	size_t size;
	time_t ctime;
	time_t atime;
	time_t mtime;
	mode_t mode;
	gid_t gid;
	uid_t uid;
	nlink_t nlink;
	folder* parent;
	bool is_folder;
public:
	entry(string n, mode_t m, bool f);
	bool isfolder();
	void get_attr(struct stat & stbuf);
	void set_attr(struct stat & stbuf, int to_set);
	const fuse_ino_t& get_ino();
	const string& get_name();
	void rename(const string & newname);
	void set_parent(folder* newparent);
	void modify();
	void onremove();
	virtual ~entry(){};	// miert nem lehet tisztan virtualis?
	friend ostream& operator<<(ostream& os, const entry& out);
	friend istream& operator>>(istream& os, entry& in);
};

class folder: public entry{
	list<entry *> childs;
public:
	folder(string n);
	int childssize();
	void addchild(entry* newentry);
	list<entry* >::const_iterator getchilditer();
	entry* getchild(list<entry* >::const_iterator & diriter);
	entry* getchild(const string& name);
	void removechild(entry* toremove);
	~folder();
};

class file: public entry{
	char* data;
	size_t alldata;	// the allocated size of the data, this is just for avoid lots of realloc
public:
	file(string n);
	const char* get_data();
	size_t read(const char* & buf, size_t size, off_t off);
	size_t write(const char* buf, size_t size, off_t off);
	void allocate(off_t offset, off_t length);
	~file();
};


class gd_cache{
private:
	static list<entry *> all;	// static variables, should be initialized outside
	static fuse_ino_t inonext;
public:
	static void init(const string& filename);
	static fuse_ino_t getnextavailableino();
	static void addnewentry(entry* newino);
	static entry* getentry(fuse_ino_t ino);
	static void removeentry(entry* toremove);
	static void save(const string& filename);
};

#endif // _gd_cache_h_