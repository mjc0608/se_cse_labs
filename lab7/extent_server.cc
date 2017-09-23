// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() 
{
  im = new inode_manager();
  pthread_mutex_init(&alloc_lock, NULL);
}

int extent_server::create(uint32_t type, extent_protocol::extentid_t &id)
{
  // alloc a new inode and return inum
  printf("extent_server: create inode\n");
  pthread_mutex_lock(&alloc_lock);
  id = im->alloc_inode(type);
  pthread_mutex_unlock(&alloc_lock);

  return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  id &= 0x7fffffff;
  
  const char * cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);
  
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  printf("extent_server: get %lld\n", id);

  id &= 0x7fffffff;

  int size = 0;
  char *cbuf = NULL;

  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }

  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  printf("extent_server: getattr %lld\n", id);

  id &= 0x7fffffff;
  
  extent_protocol::attr attr;
  memset(&attr, 0, sizeof(attr));
  im->getattr(id, attr);
  a = attr;

  return extent_protocol::OK;
}

int extent_server::setattr(extent_protocol::extentid_t id, extent_protocol::attr a, int&)
{
  printf("extent_server: setattr %lld\n", id);
  printf("this fucking attr's mode is %o\n", a.mode);
  printf("this fucking attr's uid is %d", a.uid);

  id &= 0x7fffffff;

  extent_protocol::attr attr = a;
  im->setattr(id, attr);
  
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  printf("extent_server: write %lld\n", id);

  id &= 0x7fffffff;
  im->remove_file(id);
 
  return extent_protocol::OK;
}

int extent_server::commit(int, int &)
{
  printf("extent_server: commit\n");

  im->commit();
 
  return extent_protocol::OK;
}

int extent_server::version_next(int, int &)
{
  printf("extent_server: next\n");

  im->version_next();
 
  return extent_protocol::OK;
}

int extent_server::version_prev(int, int &)
{
  printf("extent_server: prev\n");

  im->version_prev();
 
  return extent_protocol::OK;
}