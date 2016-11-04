/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Header file of MP1Node class.
 **********************************/

#ifndef _MP1NODE_H_
#define _MP1NODE_H_

#include <assert.h>
#include <stdint.h>
#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "Member.h"
#include "EmulNet.h"
#include "Queue.h"

/**
 * Macros
 */
#define TREMOVE 20
#define TFAIL 5

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Message Types
 */
enum MsgTypes{
    JOINREQ,
    JOINREP,
    GOSSIP,
    DUMMYLASTMSGTYPE
};

/**
 * STRUCT NAME: MessageHdr
 *
 * DESCRIPTION: Header and content of a message
 */
typedef struct MessageHdr {
	enum MsgTypes msgType;
}MessageHdr;

struct NodeAddress{
  char addr[6];
  NodeAddress() {}
  NodeAddress(const Address& a) { memcpy(&addr, &a.addr, sizeof(addr));}
  operator Address() const {Address a; memcpy(&a.addr, &addr, sizeof(addr)); return a;}
  bool operator<(const NodeAddress& a2) const {
    return strncmp(addr, a2.addr, sizeof(addr)) < 0;
  }
  bool IsZero() const {
    int32_t *i = (int32_t *)addr;
    int16_t *s = (int16_t *)(addr + 4);
    return *i == 0 && *s == 0;
  }
  string str() const {
    char buf[64];
    snprintf(buf, sizeof(buf), "%d.%d.%d.%d:%d",  addr[0],addr[1],addr[2],
                                 addr[3], *(short*)&addr[4]);
    return string(buf);
  }
} __attribute__((packed));

struct GossipEntry {
  GossipEntry(const NodeAddress& addr, const long heartbeat) :
    addr(addr), heartbeat(heartbeat) {}
  NodeAddress addr;
  long heartbeat;
}__attribute__((packed));

struct GossipMsg {
  MessageHdr hdr;
  NodeAddress from;
  long from_heartbeat;
  int num_members;
  GossipEntry members[0];
} __attribute__((packed));

/**
 * CLASS NAME: MP1Node
 *
 * DESCRIPTION: Class implementing Membership protocol functionalities for failure detection
 */
class MP1Node {
private:
	EmulNet *emulNet;
	Log *log;
	Params *par;
	Member *memberNode;
	char NULLADDR[6];
  struct MembershipState {
    enum class Status {Alive, Dead};
//    MembershipState(Status status) : status(status) {}
    MembershipState(Status status, long heartbeat, int64_t time ) :
      status(status), heartbeat(heartbeat), local_heartbeat_time(time) {}

    Status status;
    long heartbeat = 0;
    int64_t local_heartbeat_time = 0;
  };

  GossipMsg* createGossip(MsgTypes msgType, size_t max_nodes,
    size_t *msgsize,
    vector<GossipEntry> *rand_vec = NULL);
  void HandleMemberKnowledge(const NodeAddress& addr, long heartbeat);

  std::map<NodeAddress, MembershipState> members;

  static constexpr int kTimeout = 10;

public:
	MP1Node(Member *, Params *, EmulNet *, Log *, Address *);
	Member * getMemberNode() {
		return memberNode;
	}
	int recvLoop();
	static int enqueueWrapper(void *env, char *buff, int size);
	void nodeStart(char *servaddrstr, short serverport);
	int initThisNode(Address *joinaddr);
	int introduceSelfToGroup(Address *joinAddress);
	int finishUpThisNode();
	void nodeLoop();
	void checkMessages();
	bool recvCallBack(void *env, char *data, int size);
	void nodeLoopOps();
	int isNullAddress(Address *addr);
	Address getJoinAddress();
	void initMemberListTable(Member *memberNode);
	void printAddress(Address *addr);
	virtual ~MP1Node();
};

#endif /* _MP1NODE_H_ */
