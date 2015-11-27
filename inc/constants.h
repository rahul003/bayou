#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#include <string>
using namespace std;

// constants for socket connections
const int kMaxDataSize = 2000 ;          // max number of bytes we can get at once
const int kBacklog = 20;                // how many pending connections queue will hold

const int kMasterPort = 22222;
const string kPrimary = "P";    // primary server's name

// file paths``
const string kServerExecutable = "./bin/server";
const string kClientExecutable = "./bin/client";

// message templates
const string kMessageDelim = "$";
const string kInternalDelim = "|";

const string kInternalMapDelim = ";";
const string kInternalListDelim = "`";
const string kInternalWriteDelim = "^";
const string kComma = ",";
const string kSemiColon = ";";

const string kDone = "DONE";
const string kPort = "PORT";
const string kServerPort = "SPORT";
const string kIAm = "IAM";
const string kYouAre = "YOUARE";
const string kNewServer = "NEWSERVER";
const string kUrl = "URL";
const string kAntiEntropyP1 = "AEP1"; //AEP1$
const string kAntiEntropyP1Resp = "AEP1R"; //AEP1R-csn-clock(x:0,y:2)$
const string kAntiEntropyP2 = "AEP2"; 

const string kServer = "SERVER";
const string kClient = "CLIENT";
const string kServerVC = "SERVERVC";
const string kWriteID = "WRITEID";
const string kRelWrites = "RELWRITES";
const string kErrDep = "ERR_DEP";
const string kErrKey = "ERR_KEY";

// sleep values
const time_t kGeneralSleep = 1000 * 1000;
const time_t kBusyWaitSleep = 500 * 1000;
const time_t kAntiEntropyInterval = 1000 * 1000;

//test commands
const string kJoinServer = "joinServer";
const string kRetireServer = "retireServer";
const string kJoinClient = "joinClient";
const string kBreakConnection = "breakConnection";
const string kRestoreConnection = "restoreConnection";

const string kPut = "put";
const string kUndo = "undo";
const string kGet = "get";
const string kDelete = "delete";

const string kStabilize = "stabilize";
const string kPause = "pause";
const string kStart = "start";

// timeout values
const timeval kReceiveTimeoutTimeval = {
    0, // tv_sec
    500 * 1000 //tv_usec (microsec)
};

const timeval kSelectTimeoutTimeval = {
    0, // tv_sec
    500 * 1000 //tv_usec (microsec)
};

#endif //CONSTANTS_H_
