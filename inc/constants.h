#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#include <string>
using namespace std;

// constants for socket connections
const int kMaxDataSize = 2000 ;          // max number of bytes we can get at once
const int kBacklog = 20;                // how many pending connections queue will hold

// file paths
const string kServerExecutable = "../bin/server";
const string kClientExecutable = "../bin/client";

// message templates
const string kMessageDelim = "$";
const string kInternalDelim = "-";

const string kServerPort = "SPORT";
const string kIAm = "IAM";
const string kYouAre = "YOUARE";
const string kNewServer = "NEWSERVER";

const string kServer = "SERVER";
const string kClient = "CLIENT";

// sleep values
const time_t kGeneralSleep = 1000 * 1000;
const time_t kBusyWaitSleep = 500 * 1000;

//test commands
const string kJoinServer = "joinServer";
const string kRetireServer = "retireServer";
const string kJoinClient = "joinClient";
const string kBreakConnection = "breakConnection";
const string kRestoreConnection = "restoreConnection";

const string kPut = "put";
const string kGet = "get";
const string kDelete = "delete";

const string kStabilize = "stabilize";
const string kPause = "pause";
const string kStart = "start";


#endif //CONSTANTS_H_
