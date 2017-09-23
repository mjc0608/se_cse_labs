// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <list>
#include <iterator>

yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

std::string 
yfs_client::dl2s(std::list<dirent> &list) {
    std::ostringstream ost;
    for (std::list<dirent>::iterator it=list.begin(); it!=list.end(); it++) {
        ost << name_encode(it->name) << ' ';
        ost << it->inum << ' ';
    }
    return ost.str();
}

std::string
yfs_client::name_encode(std::string &name) {
    std::string tmp;
    for (int i=0; i<name.length(); i++) {
        tmp+='a'+name[i]/16;
        tmp+='a'+name[i]%16;
    }
    return tmp;
}

std::string
yfs_client::name_decode(std::string &xname) {
    std::string name;
    for (int i=0; i<xname.length()/2; i++) {
        int a = xname[2*i]-'a';
        int b = xname[2*i+1]-'a';
        char res = a*16+b;
        name+=res;
    }
    return name;
}


bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir or symlink\n", inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool
yfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SLINK) {
        printf("issymlink: %lld is a symlink\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir or file\n", inum);
    return false;
}

bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    return !isfile(inum) && !issymlink(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    std::string buf;
    extent_protocol::attr a;

    r = ec->getattr(ino, a);
    if (r!=OK) return r;

    if (a.size==size) {
        return r;
    }
    else if (a.size>size) {
        r = ec->get(ino, buf);

        if (r!=OK) return r;

        buf=buf.substr(0,size);
        r = ec->put(ino, buf);
    }
    else {
        r = ec->get(ino, buf);

        if (r!=OK) return r;

        std::string tmp="";
        for (int i=a.size; i<size; i++) {
            tmp+='\0';
        }
        buf+=tmp;

        r = ec->put(ino, buf);
    }

    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    printf("jiacheng: create file: %s\n", name);
    fflush(stdout);

    extent_protocol::attr pa;
    r = ec->getattr(parent, pa);

    if (r!=OK) return r;
    if (pa.type!=extent_protocol::T_DIR) {
        r = NOENT;
        return r;
    }

    std::list<dirent> list;
    std::string str_name = std::string(name);
    r = readdir(parent, list);
    if (r!=OK) return NOENT;
    for (std::list<dirent>::iterator it=list.begin(); it!=list.end(); it++) {
        if (it->name==str_name) return EXIST;
    }

    extent_protocol::extentid_t eid;
    r = ec->create(extent_protocol::T_FILE, eid);
    if (r!=OK) return r;

    dirent d;
    d.inum = eid;
    d.name = str_name;
    list.push_back(d);
    
    ec->put(parent, dl2s(list));
    ino_out = eid;

    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    extent_protocol::attr pa;
    r = ec->getattr(parent, pa);

    if (r!=OK) return r;
    if (pa.type!=extent_protocol::T_DIR) {
        r = NOENT;
        return r;
    }

    std::list<dirent> list;
    std::string str_name = std::string(name);
    r = readdir(parent, list);
    if (r!=OK) return NOENT;
    for (std::list<dirent>::iterator it=list.begin(); it!=list.end(); it++) {
        if (it->name==str_name) return EXIST;
    }

    extent_protocol::extentid_t eid;
    r = ec->create(extent_protocol::T_DIR, eid);
    if (r!=OK) return r;

    dirent d;
    d.inum = eid;
    d.name = std::string(name);
    list.push_back(d);
    
    ec->put(parent, dl2s(list));
    ino_out = eid;

    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

    extent_protocol::attr pa;
    r = ec->getattr(parent, pa);

    if (r!=OK) return r;
    if (pa.type!=extent_protocol::T_DIR) {
        r = NOENT;
        return r;
    }

    std::list<dirent> list;
    std::string str_name = std::string(name);
    r = readdir(parent, list);
    if (r!=OK) return NOENT;
    for (std::list<dirent>::iterator it=list.begin(); it!=list.end(); it++) {
        if (it->name==str_name) {
            found = true;
            ino_out = it->inum;
            break;
        }
    }

    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

    std::string buf;
    extent_protocol::attr a;

    r = ec->getattr(dir, a);
    if (r!=OK) return r;
    if (a.type == extent_protocol::T_FILE) {
        r = IOERR;
        return r;
    }

    r = ec->get(dir, buf);
    if (r!=OK) return r;

    std::istringstream ist;
    ist.str(buf);
    while (ist) {
        std::string tmp, name;
        inum ino;

        ist >> tmp;
        if (!ist || ist.eof()) break;
        name = name_decode(tmp);
        ist >> ino;
        if (!ist || ist.eof()) break;

        dirent d;
        d.name = name;
        d.inum = ino;

        list.push_back(d);
    }

    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */

    // std::cout << "jiacheng: read: " << ino << ", size: " << size << ", off: " << off << std::endl;
    std::string buf;
    if (ec->get(ino, buf)!=OK) return IOERR;

    // std::cout << "jiacheng: read data(all): " << buf << std::endl;

    if (off+size<buf.length()) {
        data = buf.substr(off, size);
    }
    else if (off<buf.length()) {
        data = buf.substr(off, buf.length());
    }
    else {
        data = "";
    }

    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    
    // std::cout << "jiacheng: write: " << ino << ", size: " << size << ", off: " << off << std::endl;  
    // printf("jiacheng: write data(s): %s\n", data);

    std::string buf;
    if (ec->get(ino, buf)!=OK) return IOERR;
    std::string str_data(data,data+size);
    // std::cout << "jiacheng: write data: " << str_data << std::endl;

    int delta = 0;
    if (off>=buf.length()) {
        for (int i=buf.length(); i<off; i++) {
            buf+='\0';
        }
        delta = off-buf.length();
        buf += str_data;
    }
    else if (off<buf.length() && off+size>buf.length()) {
        buf = buf.substr(0,off);
        buf += str_data;
    }
    else {
        buf = buf.substr(0, off) + str_data + buf.substr(off+size, buf.length());
    }

    if (ec->put(ino, buf)!=OK) return IOERR;
    bytes_written = size + delta;

    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    extent_protocol::attr pa;
    r = ec->getattr(parent, pa);

    if (r!=OK) return r;
    if (pa.type!=extent_protocol::T_DIR) {
        r = NOENT;
        return r;
    }

    std::list<dirent> list;
    std::string str_name = std::string(name);
    r = readdir(parent, list);
    if (r!=OK) return NOENT;
    for (std::list<dirent>::iterator it=list.begin(); it!=list.end(); it++) {
        if (it->name==str_name) {
            r = ec->remove(it->inum);
            if (r!=OK) return r;
            list.erase(it);
            break;
        }
    }
    
    ec->put(parent, dl2s(list));

    return r;
}

int
yfs_client::symlink(inum parent, const char *name, const char *link, inum &ino)
{
    int r = OK;

    extent_protocol::attr pa;
    r = ec->getattr(parent, pa);

    if (r!=OK) return r;
    if (pa.type!=extent_protocol::T_DIR) {
        r = NOENT;
        return r;
    }

    std::list<dirent> list;
    std::string str_name = std::string(name);
    r = readdir(parent, list);
    if (r!=OK) return NOENT;
    for (std::list<dirent>::iterator it=list.begin(); it!=list.end(); it++) {
        if (it->name==str_name) return EXIST;
    }

    extent_protocol::extentid_t eid;
    r = ec->create(extent_protocol::T_SLINK, eid);
    if (r!=OK) return r;

    dirent d;
    d.inum = eid;
    d.name = str_name;
    list.push_back(d);
    
    r = ec->put(parent, dl2s(list));
    if (r!=OK) return r;

    size_t written_size;
    r = write(eid, strlen(link), 0, link, written_size);
    if (r!=OK) return r;

    ino = eid;
    
    return r;
}

int
yfs_client::readlink(inum ino, std::string & link) {
    int r = OK;

    std::string buf;
    if (ec->get(ino, buf)!=OK) return NOENT;

    link = buf;
    return r;
}