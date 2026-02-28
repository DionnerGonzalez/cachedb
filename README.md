cachedb
A lightweight, fast, in-memory key-value store written in C with TCP server support and optional disk persistence. Inspired by Redis but built from scratch to be minimal and embeddable.
Features
In-memory hash table with open addressing
TCP server with a simple text protocol
GET, SET, DEL, EXISTS, KEYS, FLUSH commands
TTL (time-to-live) per key
Append-only file (AOF) persistence
Single-threaded event loop, no external dependencies
Build
make
Requires a C99-compatible compiler and POSIX sockets.
Usage
Start the server:
./cachedb --port 6388 --persist ./data.aof
Connect with any TCP client (e.g., netcat):
nc localhost 6388
Commands
SET key value [EX seconds]
GET key
DEL key
EXISTS key
KEYS
FLUSH
QUIT
Examples
SET name alice
+OK
GET name
$5
alice
SET session abc123 EX 60
+OK
EXISTS session
:1
DEL name
:1
KEYS
*1
$7
session
FLUSH
+OK
Protocol
The protocol is line-based and loosely inspired by RESP (Redis Serialization Protocol):
+ prefix for simple strings
- prefix for errors
: prefix for integers
$N\r\ndata\r\n for bulk strings
*N\r\n... for arrays
Persistence
When --persist <file> is given, every write command is appended to the AOF file. On startup the file is replayed to restore state.
License
MIT
