/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 *				Definition of MP1Node class functions.
 **********************************/

#include <algorithm>
#include <assert.h>
#include "MP1Node.h"
/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

bool operator==(const Address& a1, const NodeAddress& a2) {
  return memcmp(a1.addr, a2.addr, sizeof(a1.addr)) == 0;
}
bool operator==(const NodeAddress& a1, const Address& a2) {
  return memcmp(a1.addr, a2.addr, sizeof(a1.addr)) == 0;
}

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 *				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
      return false;
    }
    else {
      return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 *				All initializations routines for a member.
 *				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	//int id = *(int*)(&memberNode->addr.addr);
	//int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    const int64_t curr = par->getcurrtime();
    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
        members.emplace(memberNode->addr, MembershipState(MembershipState::Status::Alive,memberNode->heartbeat, curr));
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
   return 0;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 *				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
      return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
      return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    memberNode->memberList.clear();
    for (auto& m : members) {
      if (m.second.status == MembershipState::Status::Alive) {
        MemberListEntry mb(m.first.id(), m.first.port(), m.second.heartbeat, m.second.local_heartbeat_time);
        memberNode->memberList.emplace_back(mb);
      }
    }

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
      ptr = memberNode->mp1q.front().elt;
      size = memberNode->mp1q.front().size;
      memberNode->mp1q.pop();
      recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

GossipMsg* MP1Node::createGossip(MsgTypes msgType, size_t max_nodes,
  size_t *msgsize,
  vector<GossipEntry> *rand_vec) {

  vector<GossipEntry> mem_vec;
  for_each(members.begin(), members.end(),
      [&mem_vec] (const pair<NodeAddress,MembershipState>& m) {
      if (m.second.status == MembershipState::Status::Alive) {
      mem_vec.emplace_back(m.first, m.second.heartbeat);
      }
      });
  const int num_init_nodes = min(max_nodes, mem_vec.size());
  *msgsize = sizeof(GossipMsg) + sizeof(GossipEntry)*num_init_nodes;
  GossipMsg *msg = (GossipMsg*)malloc(*msgsize * sizeof(char));
  msg->hdr.msgType = msgType;
  msg->from = memberNode->addr;
  msg->from_heartbeat = memberNode->heartbeat;
  msg->num_members = num_init_nodes;
  std::random_shuffle(mem_vec.begin(), mem_vec.end());
  for(int i = 0; i < num_init_nodes; i++) {
    assert(!mem_vec[i].addr.IsZero());
    msg->members[i] = mem_vec[i];
  }
  if (rand_vec) {
    *rand_vec = move(mem_vec);
  }
  return msg;
}

void MP1Node::HandleMemberKnowledge(const NodeAddress& addr, long heartbeat) {
  if (addr == memberNode->addr) {
    return;
  }
  auto it = members.find(addr);
  const int64_t curr = par->getcurrtime();
  if (it == members.end()) {
    Address mem_addr = addr;
    log->logNodeAdd(&memberNode->addr, &mem_addr);
    //log->LOG(&memberNode->addr, "Adding %s with heartbeat %d", addr.str().c_str(), heartbeat);
    members.emplace(addr, MembershipState(MembershipState::Status::Alive, heartbeat, curr));
  } else if (heartbeat > it->second.heartbeat) {
    //log->LOG(&memberNode->addr, "Updating %s heartbeat to %ld from %ld", addr.str().c_str(), heartbeat, it->second.heartbeat);
    it->second.heartbeat =heartbeat;
    it->second.local_heartbeat_time = curr;
  } else {
    //log->LOG(&memberNode->addr, "Skipping %s heartbeat %ld lower than %ld", addr.str().c_str(), heartbeat, it->second.heartbeat);
  }
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	/*
	 * Your code goes here
	 */
  const MessageHdr* hdr = (const MessageHdr*)data;
  const int64_t curr = par->getcurrtime();
  switch (hdr->msgType) {
    case JOINREQ:
    {
      NodeAddress* addr = (NodeAddress*)(hdr + 1);
      assert(sizeof(Address) == sizeof(addr->addr));
      assert(sizeof(NodeAddress) == sizeof(Address));
      long *pHeartbeat = (long*)(((char*)(hdr + 1)) + 1 + sizeof(Address));
      //log->LOG(&memberNode->addr, "Adding %s with heartbeat %d", addr->str().c_str(), *pHeartbeat);
      members.emplace(*addr, MembershipState(MembershipState::Status::Alive, *pHeartbeat, curr));
      Address addr_tmp = *addr;
      log->logNodeAdd(&memberNode->addr, &addr_tmp);
      size_t msgsize = 0;
      GossipMsg *msg = createGossip(JOINREP, 5, &msgsize);
      emulNet->ENsend(&memberNode->addr, &addr_tmp, (char *)msg, msgsize);
      free(msg);
    }
    break;
    case JOINREP:
    {
      memberNode->inGroup = true;
      GossipMsg *msg = (GossipMsg *) data;
      members.emplace(memberNode->addr, MembershipState(MembershipState::Status::Alive, memberNode->heartbeat, curr));
      for (int i = 0; i < msg->num_members; i++) {
        HandleMemberKnowledge(msg->members[i].addr, msg->members[i].heartbeat);
      }
    }
    break;
    case GOSSIP:
    {
      GossipMsg *msg = (GossipMsg*)data;
      HandleMemberKnowledge(msg->from, msg->from_heartbeat);
      for(int i = 0; i < msg->num_members; i++) {
        HandleMemberKnowledge(msg->members[i].addr, msg->members[i].heartbeat);
      }
    }
    break;
    default:
    assert(0 && "unknown message type");
  }
  return true; //unused
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 *				the nodes
 *				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {
  memberNode->heartbeat++;
  //log->LOG(&memberNode->addr, "my heartbeat = %d", memberNode->heartbeat);
  const int64_t curr = par->getcurrtime();
  auto my_it = members.find(memberNode->addr);
  assert(my_it != members.end());
  my_it->second.heartbeat = memberNode->heartbeat;
  my_it->second.local_heartbeat_time = curr;

  vector<NodeAddress> to_del;

  for (auto& m : members) {
    if (m.second.status == MembershipState::Status::Alive) {
      if (curr - m.second.local_heartbeat_time > kTimeout) {
        m.second.status = MembershipState::Status::Dead;
      }
    } else {
      if (curr - m.second.local_heartbeat_time > 2*kTimeout) {
        to_del.emplace_back(m.first);
      }
    }
  }
  for (auto& a : to_del) {
    Address a_tmp = a;
    log->logNodeRemove(&memberNode->addr, &a_tmp);
    size_t erased = members.erase(a);
    assert(erased);
  }

  vector<GossipEntry> rand_vec;
  size_t msgsize = 0;
  GossipMsg *msg = createGossip(GOSSIP, 5 /* number of member entries to transmit */, &msgsize, &rand_vec);
  // Pick a different set of nodes. For it is not so useful to tell people what
  // we know of them.
  int start_index = max(0UL, rand_vec.size() - 2 /* number of nodes to transmit to */);
  for (size_t i = start_index; i < rand_vec.size(); i++) {
    Address a = rand_vec[i].addr;
    //log->LOG(&memberNode->addr, "sending gossip to %s", rand_vec[i].addr.str().c_str());
    emulNet->ENsend(&memberNode->addr, &a, (char *)msg, msgsize);
  }
  free(msg);

  return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
  members.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                 addr->addr[3], *(short*)&addr->addr[4]) ;
}
