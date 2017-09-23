// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&map_lock, NULL);
  pthread_mutex_init(&big_lock, NULL);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab4 code goes here

  pthread_mutex_lock(&map_lock);
  if (locks.find(lid)==locks.end()) {
    pthread_mutex_t *lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(lock, NULL);
    locks.insert(std::pair<lock_protocol::lockid_t, pthread_mutex_t*>(lid, lock));

    pthread_mutex_unlock(&map_lock);

    pthread_mutex_lock(lock);
    r = lock_protocol::STATUS_LOCKED;
    nacquire++;
  }
  else {
    pthread_mutex_unlock(&map_lock);
    pthread_mutex_t *lock = locks[lid];
    pthread_mutex_lock(lock);
    r = lock_protocol::STATUS_LOCKED;
    nacquire++;
  }

  // pthread_mutex_lock(&big_lock);

  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab4 code goes here

  if (locks.find(lid)!=locks.end()) {
    pthread_mutex_t *lock = locks[lid];
    pthread_mutex_unlock(lock);
    r = lock_protocol::STATUS_LOCKED;
    nacquire--;
  }
  else {
    ret = lock_protocol::NOENT;
  }

  // pthread_mutex_unlock(&big_lock);

  return ret;
}
