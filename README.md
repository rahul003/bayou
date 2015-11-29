Bayou
==============
### Description:
This program is an implementation of the Bayou protocol for weakly replicated, eventually consistent distributed database. The implementation is based on the protocol desciption and session guarantees discussed in the following three papers:

1. [Managing Update Conflicts in Bayou, a Weakly Connected Replicated Storage System](http://zoo.cs.yale.edu/classes/cs422/2013/bib/terry95managing.pdf)
2. [Flexible Update Propagation for Weakly Consistent Replication](http://joose.it/blog/wp-content/uploads/2011/03/Bayou-updates-propagation.pdf)
3. [Session Guarantees for Weakly Consistent Replicated Data](http://www.cs.utexas.edu/~lorenzo/corsi/cs380d/papers/SessionGuaranteesBayou.pdf)

The protocol is implemented in a dummy setting where multiple clients try to update/read a shared playlist of *(song, URL)* tuples. The read/write/delete requests are sent by a client to one of the many servers that are available in the system. A client may send different requests to different servers in a session or across multiple sessions. The servers are only intermittently connected. The protocol is designed to guarantee eventual consistency.

### Build instructions:
```cd``` to the project directory, and type ```make```. It should create 3 executables named ``master, client, server`` in the ```bin``` folder. Type ```make clean``` to delete all executables and logfiles. Create a folder named ``log`` in the project directory where the logs of servers will be created during execution. 

### Sample Tests:
Sample test cases are included in `tests` folder. Description of various commands that can be specified in the tests are discussed in the `Bayou.pdf`. To run `test5.test`, execute the following command in the project directory
```sh
./bin/master < ./tests/test5.test`
```
The answer to each test case is provided as a corresponding `.ans` file in `answers` directory.

### Running instructions:
Type `./bin/master` in the project directory to run the program. The master reads commands from the standard input.
stdin.

### Debug mode
By default, the debug mode is on. Execution will be accompanied with a deluge of debugging messages on the terminal. These debug messages show the progress of the protocol. Debugging can be turned off by removing `#define DEBUG` from all the `.cpp` files (or selectively from those files whose debug messages need to be turned off).

### Killing processes
Should there be an unforeseen requirement, or a bug in the implementation which necessitates premature termination of the program, execute the `kill.sh` script. Simply killing the running executable might not kill all the processes (since the master program spawns multiple processes internally), or may leave ports bound.

### Note:
1. Long tests might require some time to finish. Be patient; afterall, Bayou only provides **eventual** consistency.
2. Following characters are not allowed in song names or URLs (because our implementation heavily relies on these characters for delimiting messages propogating through the system.
`$`, `|`, `;`, `` ` ``, `^`, `,`, `;`, `!` 