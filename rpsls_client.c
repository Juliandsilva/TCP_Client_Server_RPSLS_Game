#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>    /* Internet domain header */
#include <arpa/inet.h>     /* only needed on my mac */
#include <netdb.h>         /* gethostname */
#define BUFSIZE 1000   // 1000


/*
 * Search the first n characters of buf for a network newline (\r\n).
 * Return one plus the index of the '\n' of the first network newline,
 * or -1 if no network newline is found.
 * Definitely do not use strchr or other string functions to search here. (Why not?)
 */
int find_network_newline(const char * buf, int n);
int find_network_newline(const char * buf, int n) {

  for (int i = 0; i < n - 1; i++) {

    if (buf[i] == '\r') {
      if (buf[i + 1] == '\n'){

        return i + 2;
	  }
    }
  }

  return -1;
}

int main(int argc, char ** argv) {

  if (argc > 3) {
    fprintf(stderr, "to many arguments");
    exit(1);
  }

  if (argc == 1) {
    fprintf(stderr, "not enough arguments");
    exit(1);
  }

  int n;
  if (argc == 3) {
    char * next = NULL;
    n = strtol(argv[2], & next, 10);
  }

  char * hostname = argv[1];

  int soc = socket(AF_INET, SOCK_STREAM, 0);
  if (soc < 0) {
    perror("socket");
    exit(1);
  }
  struct sockaddr_in addr;

  // Allow sockets across machines.
  addr.sin_family = AF_INET;

  // The port the server will be listening on.
  if (argc == 2) {
    addr.sin_port = htons(60000);
  }

  if (argc == 3) {
    addr.sin_port = htons(60000 + n);
  }

  // Clear this field; sin_zero is used for padding for the struct.
  memset( & (addr.sin_zero), 0, 8);

  // Lookup host IP address.
  struct hostent * hp = gethostbyname(hostname);
  if (hp == NULL) {
    fprintf(stderr, "unknown host %s\n", hostname);
    exit(1);
  }

  addr.sin_addr = *((struct in_addr *) hp->h_addr);

  // Request connection to server.
  int flag = 0; // variable to see if first port failed

  if (connect(soc, (struct sockaddr * ) & addr, sizeof(addr)) == -1) {
    flag = 1;
    if (argc == 2) {
      perror("server could not bind to port 60000, will try to bind to port 60001: bind");
    }

    if (argc == 3) {
      perror("server could not bind to port 60000 + input, will try to bind to port 60001 + input: bind");
    }
  }

  if (flag == 1) { // first port failed to bind

    if (argc == 2) {
      addr.sin_port = htons(60001);

      if (connect(soc, (struct sockaddr * ) & addr, sizeof(addr)) == -1) {

        perror("server could not bind to port 60001, will exit: bind");
        close(soc);
        exit(1);
      }

    }

    if (argc == 3) {
      addr.sin_port = htons(60001 + n);

      if (connect(soc, (struct sockaddr * ) & addr, sizeof(addr)) == -1) {

        perror("server could not bind to port 60001 + input, will exit: bind");
        close(soc);
        exit(1);
      }

    }

  }

  int procedure = 0;
  char username[50];
  char gesture;
  char seding_gesture[3]; //had 1000
  int stat = 0;
  

  while (1) {
    //     int fd = accept_connection(listenfd);
    //   if (fd < 0) {
    //     continue;
    //}

    // Receive messages
    char buf[BUFSIZE] = {'\0'};
    int inbuf = 0; // How many bytes currently in buffer?
    int room = sizeof(buf); // How many bytes remaining in buffer?
    char * after = buf; // Pointer to position after the data in buf

    int nbytes;
	
    while ((nbytes = read(soc, after, room)) > 0)  {

      // Step 1: update inbuf (how many bytes were just added?)
      inbuf += nbytes;

      int where;

      // Step 2: the loop condition below calls find_network_newline
      // to determine if a full line has been read from the client.
      // Your next task should be to implement find_network_newline
      // (found at the bottom of this file).
      //
      // Note: we use a loop here because a single read might result in
      // more than one full line.
      while ((where = find_network_newline(buf, inbuf)) > 0) {
        // Step 3: Okay, we have a full line.
        // Output the full line, not including the "\r\n",
        // using print statement below.
        // Be sure to put a '\0' in the correct place first;
        // otherwise you'll get junk in the output.

        buf[where - 2] = '\0';
        procedure++;

        if (stat == 1) {
          printf("number of games won %s\n", buf);
          close(soc);
          return 0;

        }

        if ((procedure > 1) && ((procedure % 2) == 1)) { // checking to see if we recieved game stats
          if ((buf[0] >= '0') && (buf[0] <= '9')) {

            stat = 1;
            printf("number of games played %s\n", buf);
          }

        }
        if (stat == 0) {
          if ((procedure > 1) && ((procedure % 2) == 1)) {
            printf("you %s\n",buf);
            fprintf(stderr, "%s\n", buf);
          } else {
            printf("%s\n", buf);
          }
        }
        if (procedure == 1) { //enter username
          scanf("%s", username);
          int p = strlen(username);
          username[p] = '\r';
          username[p + 1] = '\n';
          //              username[p + 2] = '\0';
          write(soc, username, p + 2); //including null terminator
          //      sleep(10);
        }

        if ((procedure % 2 == 0)) { //enter gesture

          do {
			
            scanf(" %c", &gesture);

            if ((gesture == 'r') || (gesture == 'p') || (gesture == 's') || (gesture == 'S') || (gesture == 'l') || (gesture == 'e')) {
              break;
            }
	//		printf("wrong gesture please enter either r,p,s,l,S or e");

          } while (1);

          //     printf("hi\n");

          seding_gesture[0] = gesture;
          seding_gesture[1] = '\r';
          seding_gesture[2] = '\n';
          //      seding_gesture[3] = '\0';

          write(soc, seding_gesture, 3); //not including null terminator
          memset(seding_gesture, '\0', 3);
          //      gesture = '\0';

        }

        //if (((procedure % 2 == 1) && (procedure > 1))){                   //enter gesture

        // Note that we could have also used write to avoid having to
        // put the '\0' in the buffer. Try using write later!

        // Step 4: update inbuf and remove the full line from the buffer
        // There might be stuff after the line, so don't just do inbuf = 0.

        inbuf -= where;

        // You want to move the stuff after the full line to the beginning
        // of the buffer.  A loop can do it, or you can use memmove.
        // memmove(destination, source, number_of_bytes)

        memmove(buf, buf + where, where);
        //				printf("bottom");
      }
      // Step 5: update after and room, in preparation for the next read.


      after = buf + inbuf;

      room = BUFSIZE - inbuf;
    }
           close(soc);
  }
  //	printf("here");
	return 1;
}