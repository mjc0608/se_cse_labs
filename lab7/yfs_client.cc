// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <list>
#include <iterator>
#include <string>
#include <time.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/asn1.h>


yfs_client::yfs_client()
{
    ec = NULL;
    lc = NULL;
}


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst, const char* cert_file)
{
    ec = new extent_client(extent_dst);
    lc = new lock_client(lock_dst);
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir

    // strcpy(cert, cert_file);
    verify(cert_file, &myuid);
    std::cout << "myuid: " << myuid;
    std::ifstream ifile("./etc/passwd");
    while (ifile) {
        char c;
        std::string name, password, userroot;
        unsigned short userid, gid;
        while (!ifile.eof()) {
            ifile >> c;
            if (c==':') break;
            name += c;
        }
        while (!ifile.eof()) {
            ifile >> c;
            if (c==':') break;
            password += c;
        }
        ifile >> userid;
        ifile >> c;
        ifile >> gid;
        ifile >> c;
        ifile >> userroot;
        uid2uname.insert(std::pair<int, std::string>(userid, name));
        uname2uid.insert(std::pair<std::string, int>(name, userid));
    }

    ifile.close();
    ifile.clear();
    ifile.open("./etc/group");
    while (ifile) {
        char c;
        std::string gname, password, uname;
        int gid;
        while (!ifile.eof()) {
            ifile >> c;
            if (c==':') break;
            gname += c;
        }
        while (!ifile.eof()) {
            ifile >> c;
            if (c==':') break;
            password += c;
        }
        ifile >> gid;
        gid2gname.insert(std::pair<int, std::string>(gid, gname));
        gname2gid.insert(std::pair<std::string, int>(gname, gid));
        std::set<int> s;
        groupin.insert(std::pair<int, std::set<int> >(gid, s));
        groupin[gid].insert(gid);
        ifile >> c;
        uname = "";
        while (!ifile.eof()) {
            c = ifile.get();
            if (c==',') {
                int uid = uname2uid[uname];
                groupin[gid].insert(uid);
                uname = "";
                continue;
            }
            else if (c=='\n') {
                if (uname=="") break;
                int uid = uname2uid[uname];
                groupin[gid].insert(uid);
                break;
            }
            uname += c;
        }
    }
}

static time_t ASN1_GetTimeT(ASN1_TIME* time)
{
    struct tm t;
    const char* str = (const char*) time->data;
    size_t i = 0;

    memset(&t, 0, sizeof(t));

    if (time->type == V_ASN1_UTCTIME) /* two digit year */
    {
        t.tm_year = (str[i++] - '0') * 10 + (str[++i] - '0');
        if (t.tm_year < 70)
        t.tm_year += 100;
    }
    else if (time->type == V_ASN1_GENERALIZEDTIME) /* four digit year */
    {
        t.tm_year = (str[i++] - '0') * 1000 + (str[++i] - '0') * 100 + (str[++i] - '0') * 10 + (str[++i] - '0');
        t.tm_year -= 1900;
    }
    t.tm_mon = ((str[i++] - '0') * 10 + (str[++i] - '0')) - 1; // -1 since January is 0 not 1.
    t.tm_mday = (str[i++] - '0') * 10 + (str[++i] - '0');
    t.tm_hour = (str[i++] - '0') * 10 + (str[++i] - '0');
    t.tm_min  = (str[i++] - '0') * 10 + (str[++i] - '0');
    t.tm_sec  = (str[i++] - '0') * 10 + (str[++i] - '0');

    /* Note: we did not adjust the time based on time zone information */
    return mktime(&t);
}

int
yfs_client::verify(const char* name, unsigned short *uid)
{
    OpenSSL_add_all_algorithms();

    BIO *cabio = BIO_new_file("./cert/ca.pem", "r");
    BIO *bio = BIO_new_file(name, "r");

    X509_STORE *ca_store = X509_STORE_new();
    X509_STORE_CTX *ctx = X509_STORE_CTX_new();
    STACK_OF(X509) *ca_stack = NULL;


    X509 *ca = PEM_read_bio_X509(cabio, NULL, 0, NULL);
    X509 *user = PEM_read_bio_X509(bio, NULL, 0, NULL);
    if (ca==NULL || user==NULL) {
        return yfs_client::ERRPEM;
    }

    assert(X509_STORE_add_cert(ca_store, ca));
    assert(X509_STORE_CTX_init(ctx, ca_store, user, NULL));

    int ret = X509_verify_cert(ctx);
    if (ret!=1 && ctx->error==20) {
        // std::cout << X509_verify_cert_error_string(ctx->error) << std::endl;
        return yfs_client::EINVA;
    }
    else if (ret!=1 && ctx->error==10) {
        // std::cout << X509_verify_cert_error_string(ctx->error) << std::endl;
        return yfs_client::ECTIM;
    }

    // time 
    ASN1_TIME *sstart = X509_get_notBefore(user);
    ASN1_TIME *eend = X509_get_notAfter(user);

    time_t start = ASN1_GetTimeT(sstart);
    time_t end = ASN1_GetTimeT(eend);

    time_t curr_time = time(NULL);
    if (curr_time < start || curr_time > end) {
        return yfs_client::ECTIM;
    }

    // name
    int nNameLen = 512;  
    char csCommonName[512] = {0};  
    X509_NAME *pCommonName = NULL;  
    pCommonName = X509_get_subject_name(user);
    nNameLen = X509_NAME_get_text_by_NID(pCommonName, NID_commonName, csCommonName, nNameLen);
    if (-1 == nNameLen) assert(0);

    std::string strCommonName = std::string(csCommonName);

    // std::cout << strCommonName << std::endl;

    std::fstream ifile;
    ifile.open("./etc/passwd");

    int real_uid = -1;
    while (ifile) {
        char c;
        std::string name, password, userroot;
        unsigned short userid, gid;
        while (!ifile.eof()) {
            ifile >> c;
            if (c==':') break;
            name += c;
        }
        while (!ifile.eof()) {
            ifile >> c;
            if (c==':') break;
            password += c;
        }
        ifile >> userid;
        ifile >> c;
        ifile >> gid;
        ifile >> c;
        ifile >> userroot;
        if (name==strCommonName) {
            real_uid = userid;
            break;
        }
    }

    if (real_uid==-1) return yfs_client::ENUSE;
    *uid = real_uid;
    return yfs_client::OK;
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
    for (uint32_t i=0; i<name.length(); i++) {
        tmp+='a'+name[i]/16;
        tmp+='a'+name[i]%16;
    }
    return tmp;
}

std::string
yfs_client::name_decode(std::string &xname) {
    std::string name;
    for (uint32_t i=0; i<xname.length()/2; i++) {
        int a = xname[2*i]-'a';
        int b = xname[2*i+1]-'a';
        char res = a*16+b;
        name+=res;
    }
    return name;
}

bool
yfs_client::can_read(uint32_t uid, inum ino) {
    if (uid == 0) return true;

    extent_protocol::attr a;
    ec->getattr(ino, a);

    std::cout << "uid = " << uid << " ,file-uid = " << a.uid << " ,";
    printf("mode = %o\n", a.mode);

    if (a.uid == uid) {
        return !!(a.mode & 0x100);
    }
    else if (groupin[a.gid].count(uid)) {
        return !!(a.mode & 0x20);
    }
    else {
        return !!(a.mode & 0x4);
    }
}

bool
yfs_client::can_write(uint32_t uid, inum ino) {
    if (uid == 0) return true;

    extent_protocol::attr a;
    ec->getattr(ino, a);
    bool ret;

    std::cout << "uid = " << uid << " ,file-uid = " << a.uid << " ,";
    printf("mode = %o\n", a.mode);

    if (a.uid == uid) {
        ret = !!(a.mode & 0x80);
        return ret;
    }
    else if (groupin[a.gid].count(uid)) {
        ret = !!(a.mode & 0x10);
        return ret;
    }
    else {
        return !!(a.mode & 0x2);
    }
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

    // if (!can_read(myuid, inum)) {
    //     return yfs_client::NOPEM;
    // }

    lc->acquire(inum);

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        lc->release(inum);
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    fin.mode = a.mode;
    fin.uid = a.uid;
    fin.gid = a.gid;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    lc->release(inum);
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    // if (!can_read(myuid, inum)) {
    //     return yfs_client::NOPEM;
    // }

    lc->acquire(inum);

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;
    din.mode = a.mode;
    din.uid = (unsigned short)a.uid;
    din.gid = (unsigned short)a.gid;

release:
    lc->release(inum);
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

int
yfs_client::setattr(inum ino, filestat st, unsigned long toset, bool god)
{
    extent_protocol::attr a;
    ec->getattr(ino, a);
    printf("this fucking attr's mode is %o", a.mode);
    printf("this fucking attr's uid is %d", a.uid);

    // change all
    if (god) {
        a.mode = st.mode;
        a.uid = st.uid;
        a.gid = st.gid;
        ec->setattr(ino, a);
    }
    else {
        if (toset&4) {
            if (myuid != 0) return yfs_client::NOPEM;
            a.gid = st.gid;
        }
        if (toset&2) {
            if (myuid != 0) return yfs_client::NOPEM;
            a.uid = st.uid;
        }
        if (toset&1) {
            if (myuid != 0 && myuid!=a.uid) return yfs_client::NOPEM;
            a.mode = st.mode;
        }
        ec->setattr(ino, a);
    }
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    if (!can_write(myuid, parent)) return yfs_client::NOPEM;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    lc->acquire(parent);

    printf("jiacheng: create file: %s\n", name);
    fflush(stdout);

    extent_protocol::attr pa;
    r = ec->getattr(parent, pa);

    if (r!=OK) {
        lc->release(parent);
        return r;
    }
    if (pa.type!=extent_protocol::T_DIR) {
        r = NOENT;
        lc->release(parent);
        return r;
    }

    std::list<dirent> list;
    std::string str_name = std::string(name);
    r = fck_readdir(parent, list);
    if (r!=OK) {
        lc->release(parent);
        return NOENT;
    }
    for (std::list<dirent>::iterator it=list.begin(); it!=list.end(); it++) {
        if (it->name==str_name) {
            lc->release(parent);
            return EXIST;
        }
    }

    extent_protocol::extentid_t eid;
    r = ec->create(extent_protocol::T_FILE, eid);
    if (r!=OK) {
        lc->release(parent);
        return r;
    }

    dirent d;
    d.inum = eid;
    d.name = str_name;
    list.push_back(d);
    
    ec->put(parent, dl2s(list));
    ino_out = eid;

    filestat st;
    st.mode = mode;
    st.uid = myuid;
    st.gid = myuid;
    setattr(ino_out, st, 1, true);

    lc->release(parent);
    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    if (!can_write(myuid, parent)) return yfs_client::NOPEM;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    printf("mkdir: %llu: %s", parent, name);

    lc->acquire(parent);

    extent_protocol::attr pa;
    r = ec->getattr(parent, pa);

    if (r!=OK) {
        lc->release(parent);
        
        return r;
    }
    if (pa.type!=extent_protocol::T_DIR) {
        r = NOENT;
        lc->release(parent);
        return r;
    }

    std::list<dirent> list;
    std::string str_name = std::string(name);
    r = fck_readdir(parent, list);
    if (r!=OK) {
        lc->release(parent);
        return NOENT;
    }
    for (std::list<dirent>::iterator it=list.begin(); it!=list.end(); it++) {
        if (it->name==str_name) {
            lc->release(parent);
            return EXIST;
        }
    }

    extent_protocol::extentid_t eid;
    r = ec->create(extent_protocol::T_DIR, eid);
    if (r!=OK) {
        lc->release(parent);
        return r;
    }

    dirent d;
    d.inum = eid;
    d.name = std::string(name);
    list.push_back(d);
    
    ec->put(parent, dl2s(list));
    ino_out = eid;

    filestat st;
    st.mode = mode;
    st.uid = myuid;
    st.gid = myuid;
    setattr(ino_out, st, 1, true);

    lc->release(parent);
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    // if (!can_read(myuid, parent)) return yfs_client::NOPEM;

    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

    lc->acquire(parent); //new

    extent_protocol::attr pa;
    r = ec->getattr(parent, pa);

    if (r!=OK) {
        lc->release(parent);
        return r;
    }
    if (pa.type!=extent_protocol::T_DIR) {
        r = NOENT;
        lc->release(parent); //new
        return r;
    }

    std::list<dirent> list;
    std::string str_name = std::string(name);
    r = readdir(parent, list);
    if (r!=OK) {
        lc->release(parent);
        return NOENT;
    }
    for (std::list<dirent>::iterator it=list.begin(); it!=list.end(); it++) {
        if (it->name==str_name) {
            found = true;
            ino_out = it->inum;
            break;
        }
    }

    lc->release(parent); //new

    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    if (!can_read(myuid, dir)) return yfs_client::NOPEM;


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
yfs_client::fck_readdir(inum dir, std::list<dirent> &list)
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

    if (!can_read(myuid, ino)) return yfs_client::NOPEM;


    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */

    lc->acquire(ino);

    // std::cout << "jiacheng: read: " << ino << ", size: " << size << ", off: " << off << std::endl;
    std::string buf;
    if (ec->get(ino, buf)!=OK) {
        lc->release(ino);
        return IOERR;
    }

    // std::cout << "jiacheng: read data(all): " << buf << std::endl;

    if (off+size<buf.length()) {
        data = buf.substr(off, size);
    }
    else if (off<(off_t)buf.length()) {
        data = buf.substr(off, buf.length());
    }
    else {
        data = "";
    }

    lc->release(ino);

    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{

    if (!can_write(myuid, ino)) return yfs_client::NOPEM;


    lc->acquire(ino);

    int r = OK;

    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    
    // std::cout << "jiacheng: write: " << ino << ", size: " << size << ", off: " << off << std::endl;  
    // printf("jiacheng: write data(s): %s\n", data);

    std::string buf;
    if (ec->get(ino, buf)!=OK) {
        lc->release(ino);
        return IOERR;
    }
    std::string str_data(data,data+size);
    // std::cout << "jiacheng: write data: " << str_data << std::endl;

    int delta = 0;
    if (off>=(off_t)buf.length()) {
        for (int i=buf.length(); i<off; i++) {
            buf+='\0';
        }
        delta = off-buf.length();
        buf += str_data;
    }
    else if (off<(off_t)buf.length() && off+size>buf.length()) {
        buf = buf.substr(0,off);
        buf += str_data;
    }
    else {
        buf = buf.substr(0, off) + str_data + buf.substr(off+size, buf.length());
    }

    if (ec->put(ino, buf)!=OK) {
        lc->release(ino);
        return IOERR;
    }
    bytes_written = size + delta;

    lc->release(ino);
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{

    if (!can_write(myuid, parent)) return yfs_client::NOPEM;


    lc->acquire(parent);

    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    extent_protocol::attr pa;
    r = ec->getattr(parent, pa);

    if (r!=OK) {
        lc->release(parent);
        return r;
    }
    if (pa.type!=extent_protocol::T_DIR) {
        r = NOENT;
        lc->release(parent);
        return r;
    }

    std::list<dirent> list;
    std::string str_name = std::string(name);
    r = readdir(parent, list);
    if (r!=OK) {
        lc->release(parent);
        return NOENT;
    }
    for (std::list<dirent>::iterator it=list.begin(); it!=list.end(); it++) {
        if (it->name==str_name) {
            r = ec->remove(it->inum);
            if (r!=OK) {
                lc->release(parent);
                return r;
            }
            list.erase(it);
            break;
        }
    }
    
    ec->put(parent, dl2s(list));

    lc->release(parent);
    return r;
}

int
yfs_client::symlink(inum parent, const char *name, const char *link, inum &ino)
{

    if (!can_write(myuid, parent)) return yfs_client::NOPEM;

    lc->acquire(parent);

    int r = OK;

    extent_protocol::attr pa;
    r = ec->getattr(parent, pa);

    if (r!=OK) {
        lc->release(parent);
        return r;
    }
    if (pa.type!=extent_protocol::T_DIR) {
        r = NOENT;
        lc->release(parent);
        return r;
    }

    std::list<dirent> list;
    std::string str_name = std::string(name);
    r = readdir(parent, list);
    if (r!=OK) {
        lc->release(parent);
        return NOENT;
    }
    for (std::list<dirent>::iterator it=list.begin(); it!=list.end(); it++) {
        if (it->name==str_name) {
            lc->release(parent);
            return EXIST;
        }
    }

    extent_protocol::extentid_t eid;
    r = ec->create(extent_protocol::T_SLINK, eid);
    if (r!=OK) {
        lc->release(parent);
        return r;
    }

    dirent d;
    d.inum = eid;
    d.name = str_name;
    list.push_back(d);
    
    r = ec->put(parent, dl2s(list));
    if (r!=OK) {
        lc->release(parent);
        return r;
    }

    size_t written_size;
    r = write(eid, strlen(link), 0, link, written_size);
    if (r!=OK) {
        lc->release(parent);
        return r;
    }

    ino = eid;

    filestat st;
    st.mode = 0777;
    st.uid = myuid;
    st.gid = myuid;
    setattr(ino, st, 1, true);
    
    lc->release(parent);
    return r;
}

int
yfs_client::readlink(inum ino, std::string & link) {

    if (!can_read(myuid, ino)) return yfs_client::NOPEM;


    int r = OK;

    lc->acquire(ino);

    std::string buf;
    if (ec->get(ino, buf)!=OK) {
        lc->release(ino);
        return NOENT;
    }

    link = buf;

    lc->release(ino);

    return r;
}

int yfs_client::commit() {
    int r = OK;
    ec->commit();
    return r;
}

int yfs_client::version_next() {
    int r = OK;
    ec->version_next();
    return r;
}

int yfs_client::version_prev() {
    int r = OK;
    ec->version_prev();
    return r;
}
