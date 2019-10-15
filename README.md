P3 Botnet
-----------------------------------------------------------------
Authors: Arnór Erling, Emil Newel.
-----------------------------------------------------------------

OS:
    Works on macos, ubuntu and skel
Compile:
    When compiling run make in the same folder as the code.
-----------------------------------------------------------------
Running Server: 
First find an open port
./P3_GROUP_2 <port>

Running Client:
./Client <ip that server runs on> <the chosen port>
-----------------------------------------------------------------
Working Client Commands:

Connect to another server: 
CONNECT <ip of server> <port of server>

Get List of servers connected to your server:
LISTSERVERS

Send message to a group:
SENDMSG,name_of_group,message

Get message from a group: returns the first message from a group.
GETMSG,name_of_group
-----------------------------------------------------------------

Working Client Commands
LISTSERVERS,<FROM GROUP ID> 
 
KEEPALIVE,<No. of Messages> 

GET MSG, <GROUP ID> 

SEND MSG,<FROM GROUP ID>,<TO GROUP ID>,<Message content>

LEAVE,SERVER IP,PORT 

STATUSRESP,FROM GROUP,TO GROUP,<server, msgs held>

-----------------------------------------------------------------
