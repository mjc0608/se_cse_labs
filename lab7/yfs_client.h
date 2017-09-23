#ifndef yfs_client_h
#define yfs_client_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>
#include <map>
#include <set>


#define CA_FILE "./cert/ca.pem"
#define USERFILE	"./etc/passwd"
#define GROUPFILE	"./etc/group"


class yfs_client {
  extent_client *ec;
  lock_client *lc;
  char cert[128];
  unsigned short myuid;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST,
  			NOPEM, ERRPEM, EINVA, ECTIM, ENUSE };

  typedef int status;


  struct filestat {
  	unsigned long long size;
	unsigned long mode;
	unsigned short uid;
	unsigned short gid;
  };

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
	unsigned long mode;
	unsigned short uid;
	unsigned short gid;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
	unsigned long mode;
	unsigned short uid;
	unsigned short gid;
  };

  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  std::map<std::string, int> uname2uid;
  std::map<std::string, int> gname2gid;
  std::map<int, std::string> uid2uname;
  std::map<int, std::string> gid2gname;
  std::map<int, std::set<int> > groupin;

  static std::string filename(inum);
  static inum n2i(std::string);
  static std::string dl2s(std::list<dirent> &);
  static std::string name_encode(std::string &);
  static std::string name_decode(std::string &);
  bool can_read(uint32_t uid, inum ino);
  bool can_write(uint32_t uid, inum ino);
  int fck_readdir(inum, std::list<dirent> &);


 public:
  yfs_client();
  yfs_client(std::string, std::string, const char*);

  bool isfile(inum);
  bool isdir(inum);
  bool issymlink(inum);


  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int setattr(inum, filestat, unsigned long, bool god);
  int lookup(inum, const char *, bool &, inum &);
  int create(inum, const char *, mode_t, inum &);
  int readdir(inum, std::list<dirent> &);
  int write(inum, size_t, off_t, const char *, size_t &);
  int read(inum, size_t, off_t, std::string &);
  int unlink(inum,const char *);
  int mkdir(inum , const char *, mode_t , inum &);

  int verify(const char* cert_file, unsigned short*);
  
  /** you may need to add symbolic link related methods here.*/
  int symlink(inum, const char *, const char *, inum &);
  int readlink(inum, std::string &);

  int commit();
  int version_prev();
  int version_next();
};

#endif 
