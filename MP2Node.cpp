/**********************************
 * FILE NAME: MP2Node.cpp
 *
 * DESCRIPTION: MP2Node class definition
 **********************************/

#include <set>
#include "MP2Node.h"
#include <iostream>

/**
 * constructor
 */
MP2Node::MP2Node(Member *memberNode, Params *par, EmulNet * emulNet, Log * log, Address * address) {
	this->memberNode = memberNode;
	this->par = par;
	this->emulNet = emulNet;
	this->log = log;
	ht = new HashTable();
	this->memberNode->addr = *address;
}

/**
 * Destructor
 */
MP2Node::~MP2Node() {
	delete ht;
	delete memberNode;
}

bool operator!=(Address& a1, Address& a2) {
  return ! (a1 == a2);
}

bool operator==(const Node& n1, const Node& n2) {
  return memcmp(n1.nodeAddress.addr, n2.nodeAddress.addr, sizeof(n1.nodeAddress.addr)) == 0;
}

/**
 * FUNCTION NAME: updateRing
 *
 * DESCRIPTION: This function does the following:
 * 				1) Gets the current membership list from the Membership Protocol (MP1Node)
 * 				   The membership list is returned as a vector of Nodes. See Node class in Node.h
 * 				2) Constructs the ring based on the membership list
 * 				3) Calls the Stabilization Protocol
 */
void MP2Node::updateRing() {
	/*
	 * Implement this. Parts of it are already implemented
	 */
	vector<Node> curMemList;
	bool change = false;

	/*
	 *  Step 1. Get the current membership list from Membership Protocol / MP1
	 */
	curMemList = getMembershipList();

	/*
	 * Step 2: Construct the ring
	 */
	// Sort the list based on the hashCode
	sort(curMemList.begin(), curMemList.end());

	/*
	 * Step 3: Run the stabilization protocol IF REQUIRED
	 */
  change = ring != curMemList;
  if (change) {
    cout << "Ring size is now: " << curMemList.size() << "\n";
    // Run stabilization protocol if the hash table size is greater than zero and if there has been a change in the ring
    stabilizationProtocol(curMemList);
  }
  ring = curMemList;
}

/**
 * FUNCTION NAME: getMemberhipList
 *
 * DESCRIPTION: This function goes through the membership list from the Membership protocol/MP1 and
 * 				i) generates the hash code for each member
 * 				ii) populates the ring member in MP2Node class
 * 				It returns a vector of Nodes. Each element in the vector contain the following fields:
 * 				a) Address of the node
 * 				b) Hash code obtained by consistent hashing of the Address
 */
vector<Node> MP2Node::getMembershipList() {
	unsigned int i;
	vector<Node> curMemList;
	for ( i = 0 ; i < this->memberNode->memberList.size(); i++ ) {
		Address addressOfThisMember;
		int id = this->memberNode->memberList.at(i).getid();
		short port = this->memberNode->memberList.at(i).getport();
		memcpy(&addressOfThisMember.addr[0], &id, sizeof(int));
		memcpy(&addressOfThisMember.addr[4], &port, sizeof(short));
		curMemList.emplace_back(Node(addressOfThisMember));
	}
	return curMemList;
}

/**
 * FUNCTION NAME: hashFunction
 *
 * DESCRIPTION: This functions hashes the key and returns the position on the ring
 * 				HASH FUNCTION USED FOR CONSISTENT HASHING
 *
 * RETURNS:
 * size_t position on the ring
 */
size_t MP2Node::hashFunction(string key) {
	std::hash<string> hashFunc;
	size_t ret = hashFunc(key);
	return ret%RING_SIZE;
}

void MP2Node::SendMsg(Message& m) {
  vector<Node> reps = findNodes(m.key);
  waiting.emplace(m.transID, WaitState(m, reps, par->getcurrtime()));
  for (auto& r : reps) {
    SendMsg(r.nodeAddress, m);
  }
}

void MP2Node::SendMsg(Address& a, Message& m) {
  string msg = m.toString();
  switch (m.type) {
    case CREATE:
    case READ:
    case UPDATE:
    case DELETE:
    break;
    default:
    assert(m.type == REPLY || m.type == READREPLY);
  }
  emulNet->ENsend(&memberNode->addr, &a, (char *)msg.c_str(), msg.size());
}

/**
 * FUNCTION NAME: clientCreate
 *
 * DESCRIPTION: client side CREATE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientCreate(string key, string value) {
  Message m(++trans_id, memberNode->addr, CREATE, key, value);
  SendMsg(m);
}

/**
 * FUNCTION NAME: clientRead
 *
 * DESCRIPTION: client side READ API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientRead(string key){
  Message m(++trans_id, memberNode->addr, READ, key);
  SendMsg(m);
}

/**
 * FUNCTION NAME: clientUpdate
 *
 * DESCRIPTION: client side UPDATE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientUpdate(string key, string value){
  Message m(++trans_id, memberNode->addr, UPDATE, key, value);
  SendMsg(m);
}

/**
 * FUNCTION NAME: clientDelete
 *
 * DESCRIPTION: client side DELETE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientDelete(string key){
  Message m(++trans_id, memberNode->addr, DELETE, key);
  SendMsg(m);
}

/**
 * FUNCTION NAME: createKeyValue
 *
 * DESCRIPTION: Server side CREATE API
 * 			   	The function does the following:
 * 			   	1) Inserts key value into the local hash table
 * 			   	2) Return true or false based on success or failure
 */
bool MP2Node::createKeyValue(string key, string value, ReplicaType replica) {
	// Insert key, value, replicaType into the hash table
  bool ret = ht->create(key, value);
  return ret;
}

/**
 * FUNCTION NAME: readKey
 *
 * DESCRIPTION: Server side READ API
 * 			    This function does the following:
 * 			    1) Read key from local hash table
 * 			    2) Return value
 */
string MP2Node::readKey(string key) {
	// Read key from local hash table and return value
  return ht->read(key);
}

/**
 * FUNCTION NAME: updateKeyValue
 *
 * DESCRIPTION: Server side UPDATE API
 * 				This function does the following:
 * 				1) Update the key to the new value in the local hash table
 * 				2) Return true or false based on success or failure
 */
bool MP2Node::updateKeyValue(string key, string value, ReplicaType replica) {
	// Update key in local hash table and return true or false
  return ht->update(key, value);
}

/**
 * FUNCTION NAME: deleteKey
 *
 * DESCRIPTION: Server side DELETE API
 * 				This function does the following:
 * 				1) Delete the key from the local hash table
 * 				2) Return true or false based on success or failure
 */
bool MP2Node::deletekey(string key) {
	// Delete the key from the local hash table
  return ht->deleteKey(key);
}

void MP2Node::log_c_failure(Message& m) {
  switch (m.type) {
    case CREATE:
      log->logCreateFail(&memberNode->addr, true, m.transID, m.key, m.value);
      break;
    case READ:
      log->logReadFail(&memberNode->addr, true, m.transID, m.key);
      break;
    case UPDATE:
      log->logUpdateFail(&memberNode->addr, true, m.transID, m.key, m.value);
      break;
    case DELETE:
      log->logDeleteFail(&memberNode->addr, true, m.transID, m.key);
      break;
    default:
    assert(0);
  }
}

void MP2Node::log_c_success(Message& m) {
  switch (m.type) {
    case CREATE:
      log->logCreateSuccess(&memberNode->addr, true, m.transID, m.key, m.value);
      break;
    case READ:
      log->logReadSuccess(&memberNode->addr, true, m.transID, m.key, m.value);
      break;
    case UPDATE:
      log->logUpdateSuccess(&memberNode->addr, true, m.transID, m.key, m.value);
      break;
    case DELETE:
      log->logDeleteSuccess(&memberNode->addr, true, m.transID, m.key);
      break;
    default:
    assert(0);
  }
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: This function is the message handler of this node.
 * 				This function does the following:
 * 				1) Pops messages from the queue
 * 				2) Handles the messages according to message types
 */
void MP2Node::checkMessages() {
	/*
	 * Implement this. Parts of it are already implemented
	 */
	char * data;
	int size;

	/*
	 * Declare your local variables here
	 */

	// dequeue all messages and handle them
	while ( !memberNode->mp2q.empty() ) {
		/*
		 * Pop a message from the queue
		 */
		data = (char *)memberNode->mp2q.front().elt;
		size = memberNode->mp2q.front().size;
		memberNode->mp2q.pop();

		string message(data, data + size);
    Message m(message);

    bool res;

    switch (m.type) {
      case CREATE:
      {
        res=createKeyValue(m.key, m.value, m.replica);
        Message reply(m.transID, memberNode->addr, REPLY, res);
        if (res) {
          log->logCreateSuccess(&memberNode->addr, false, m.transID, m.key, m.value);
        } else {
          log->logCreateFail(&memberNode->addr, false, m.transID, m.key, m.value);
        }
        SendMsg(m.fromAddr, reply);
      }
      break;
      case READ:
      {
        string val = readKey(m.key);
        if (!val.empty()) {
          log->logReadSuccess(&memberNode->addr, false, m.transID, m.key, val);
        } else {
          log->logReadFail(&memberNode->addr, false, m.transID, m.key);
        }
        Message reply(m.transID, memberNode->addr, val);
        SendMsg(m.fromAddr, reply);
      }
      break;
      case UPDATE:
      {
        if (m.transID == 0x0fffffff) {
          ht->hashTable[m.key] = m.value;
          break;
        }
        res=updateKeyValue(m.key, m.value, m.replica);
        if (res) {
          log->logUpdateSuccess(&memberNode->addr, false, m.transID, m.key, m.value);
        } else {
          log->logUpdateFail(&memberNode->addr, false, m.transID, m.key, m.value);
        }
        Message reply(m.transID, memberNode->addr, REPLY, res);
        SendMsg(m.fromAddr, reply);
      }
      break;
      case DELETE:
      {
        res=deletekey(m.key);
        if (res) {
          log->logDeleteSuccess(&memberNode->addr, false, m.transID, m.key);
        } else {
          log->logDeleteFail(&memberNode->addr, false, m.transID, m.key);
        }
        Message reply(m.transID, memberNode->addr, REPLY, res);
        SendMsg(m.fromAddr, reply);
      }
      break;
      case REPLY:
      case READREPLY:
      {
        auto it = waiting.find(m.transID);
        assert(it != waiting.end());
        if (it->second.done) {
          break; //from switch
        }
        auto& nvec = it->second.nodes;
        Message& orig_m = it->second.msg;
        int n_wait = 0, n_good = 0, n_failed = 0;
        for (int ii = 0; ii < nvec.size(); ++ii) {
          if (nvec[ii].nodeAddress == m.fromAddr) {
            if (m.type == REPLY) {
              it->second.res[ii] = make_pair(m.success ? WaitState::NWS_GOOD : WaitState::NWS_FAILED, "");
            } else {
              it->second.res[ii] = make_pair(WaitState::NWS_GOOD, m.value);
            }
          }
          switch (it->second.res[ii].first) {
            case WaitState::NWS_WAIT:
            n_wait++;
            break;
            case WaitState::NWS_GOOD:
            n_good++;
            break;
            case WaitState::NWS_FAILED:
            n_failed++;
            break;
            default:
            assert(0);
          }
        }
        if (m.type == REPLY) {
          if (n_good >= 2) {
            assert(n_good == 2);
            log_c_success(orig_m);
            it->second.done = true;
          } else if (n_failed >= 2) {
            assert(n_failed == 2);
            log_c_failure(orig_m);
            it->second.done = true;
          } else {
            assert(n_wait >= 1);
          }
        } else {
          vector<string> vals;
          for (auto& res : it->second.res) {
            vals.emplace_back(res.second);
          }
          sort(vals.begin(), vals.end());
          assert(vals.size() == 3);
          if (!vals[1].empty() && (vals[1] == vals[0] || vals[1] == vals[2])) {
            orig_m.value = m.value;
            log_c_success(orig_m);
            it->second.done = true;
          }
          if (n_wait == 0) { 
            log_c_failure(orig_m);
            it->second.done = true;
          }
        }
      }
      break;
      default:
      assert(0);
    }
	}

  const int64_t curr = par->getcurrtime();
  for (auto it = waiting.begin(); it != waiting.end(); ) {
    if (curr > it->second.send_time + 40) {
      if (!it->second.done) {
        log_c_failure(it->second.msg);
        it->second.done = true;
      }
      it = waiting.erase(it);
    } else {
      ++it;
    }
  }
}

/**
 * FUNCTION NAME: findNodes
 *
 * DESCRIPTION: Find the replicas of the given keyfunction
 * 				This function is responsible for finding the replicas of a key
 */
vector<Node> MP2Node::findNodes(string key) {
	size_t pos = hashFunction(key);
  return findNodes(pos, ring);
}

vector<Node> MP2Node::findNodes(size_t pos, vector<Node>& ring) {
	vector<Node> addr_vec;
	if (ring.size() >= 3) {
		// if pos <= min || pos > max, the leader is the min
		if (pos <= ring.at(0).getHashCode() || pos > ring.at(ring.size()-1).getHashCode()) {
			addr_vec.emplace_back(ring.at(0));
			addr_vec.emplace_back(ring.at(1));
			addr_vec.emplace_back(ring.at(2));
		}
		else {
			// go through the ring until pos <= node
			for (int i=1; i<ring.size(); i++){
				Node addr = ring.at(i);
				if (pos <= addr.getHashCode()) {
//          cout << "First replica for " << key << " is " << i << endl;
					addr_vec.emplace_back(addr);
					addr_vec.emplace_back(ring.at((i+1)%ring.size()));
					addr_vec.emplace_back(ring.at((i+2)%ring.size()));
					break;
				}
			}
		}
	}
	return addr_vec;
}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: Receive messages from EmulNet and push into the queue (mp2q)
 */
bool MP2Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), this->enqueueWrapper, NULL, 1, &(memberNode->mp2q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue of MP2Node
 */
int MP2Node::enqueueWrapper(void *env, char *buff, int size) {
  Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

set<size_t> mem_hash(vector<Node>& ring) {
  set<size_t> ret;
  for (auto& n : ring) {
    ret.emplace(n.getHashCode());
  }
  return ret;
}

set<size_t>::iterator go_back(const set<size_t>& s, set<size_t>::iterator it) {
  if (it == s.begin()) {
    it = s.end();
  }
  return --it;
}

void add_range(vector<pair<size_t, size_t>>& vec, size_t p1, size_t p2) {
  if (p1 < p2) {
    vec.emplace_back(make_pair(p1,p2));
  } else {
    vec.emplace_back(make_pair(p1, INT64_MAX));
    vec.emplace_back(make_pair(0, p2));
  }
}

/**
 * FUNCTION NAME: stabilizationProtocol
 *
 * DESCRIPTION: This runs the stabilization protocol in case of Node joins and leaves
 * 				It ensures that there always 3 copies of all keys in the DHT at all times
 * 				The function does the following:
 *				1) Ensures that there are three "CORRECT" replicas of all the keys in spite of failures and joins
 *				Note:- "CORRECT" replicas implies that every key is replicated in its two neighboring nodes in the ring
 */
void MP2Node::stabilizationProtocol(vector<Node>& new_ring) {
  if (ring.size() < 3) {
    return;
  }

  const set<size_t> ring_set = mem_hash(ring);
  const set<size_t> new_ring_set = mem_hash(new_ring);
  for (int i = 0; i < ring.size(); ++i) {
    cout << "Member " << i << " is " << ring[i].nodeAddress.getAddress()
      << " hash=" << ring[i].getHashCode() << endl;
  }

  int my_index = -1;
  for (int i = 0; i < ring.size(); ++i) {
    if (ring[i].nodeAddress == memberNode->addr) {
      my_index = i;
      break;
    }
  }
  assert(my_index >= 0);
  
  size_t my_hash = ring[my_index].getHashCode();
  auto my_it = ring_set.find(my_hash);
  assert(my_it != ring_set.end());
  auto range_it3 = go_back(ring_set, my_it);
  auto range_it2 = go_back(ring_set, range_it3);
  auto range_it1 = go_back(ring_set, range_it2);
  vector<pair<size_t, size_t>> my_ranges;
  add_range(my_ranges, *range_it1, *range_it2);
  add_range(my_ranges, *range_it2, *range_it3);
  add_range(my_ranges, *range_it3, *my_it);
  for (auto r : my_ranges) {
    cout << r.first << " - " << r.second << endl;
    auto rs1 = findNodes(r.first, ring);
    auto rs2 = findNodes(r.first, new_ring);
    if (rs1 != rs2) {
      assert(rs1.size() == 3);
      assert(rs2.size() == 3);
      for (Node& new_rep : rs2) {
        bool found = false;
        for (Node& old_rep : rs1) {
          if (new_rep.nodeAddress == old_rep.nodeAddress) {
            found = true;
            break;
          }
        }
        if (!found) {
          Replicate(r, new_rep);
        }
      }
    }
  }
}

void MP2Node::Replicate(pair<size_t,size_t>& range, Node& n) {
	//assert(memberNode->addr != n.nodeAddress);
  auto& mp = ht->hashTable;
  cout << "Replicating " << range.first << "-" << range.second << " to " << n.nodeAddress.getAddress() << endl;
  for (auto& kv : mp) {
    size_t pos = hashFunction(kv.first);
    if (pos >= range.first && pos < range.second) {
      ReplicateKey(kv, n);
    }
  }
}

void MP2Node::ReplicateKey(const pair<string,string>& kv, Node& n) {
  Message m(0x0fffffff, n.nodeAddress, UPDATE, kv.first, kv.second);
  SendMsg(m);
}
