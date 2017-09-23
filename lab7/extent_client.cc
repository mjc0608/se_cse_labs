// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
}

// a demo to show how to use RPC
extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::getattr, eid, attr);
  return ret;
}

extent_protocol::status
extent_client::setattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  int tmp = 0;
  ret = cl->call(extent_protocol::setattr, eid, attr, tmp);
  return ret;
}

extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  ret = cl->call(extent_protocol::create, type, id);
  return ret;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  ret = cl->call(extent_protocol::get, eid, buf);
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  int t = 0;
  ret = cl->call(extent_protocol::put, eid, buf, t);
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here 
  int t = 0;
  ret = cl->call(extent_protocol::remove, eid, t);
  return ret;
}

extent_protocol::status
extent_client::commit() {
  extent_protocol::status ret = extent_protocol::OK;
  int t = 0;
  ret = cl->call(extent_protocol::commit, t, t);
  return ret;
}

extent_protocol::status
extent_client::version_next() {
  extent_protocol::status ret = extent_protocol::OK;
  int t = 0;
  ret = cl->call(extent_protocol::version_next, t, t);
  return ret;
}

extent_protocol::status
extent_client::version_prev() {
  extent_protocol::status ret = extent_protocol::OK;
  int t = 0;
  ret = cl->call(extent_protocol::version_prev, t, t);
  return ret;
}