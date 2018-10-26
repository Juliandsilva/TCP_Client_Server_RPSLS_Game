#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>



#define MAX_BACKLOG 5
#define MAX_CONNECTIONS 12
#define BUF_SIZE 128



// Server works properly by itself, try running server and then connect to it by calling the command telnet localhost 60000 on 2 other terminals, the game works fine, might have trouble if you use the client code  


int find_network_newline(const char * buf, int n);

struct sockname {
  int sock_fd;
  char * username;
};

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

/* Accept a connection. Note that a new file descriptor is created for
 * communication with the client. The initial socket descriptor is used
 * to accept connections, but the new socket is used to communicate.
 * Return the new client's file descriptor or -1 on error.
 */
int accept_connection(int fd, struct sockname * connections) {
  int user_index = 0;
  while (user_index < MAX_CONNECTIONS && connections[user_index].sock_fd != -1) {
    user_index++;
  }

  if (user_index == MAX_CONNECTIONS) {
    fprintf(stderr, "server: max concurrent connections\n");
    return -1;
  }

  int client_fd = accept(fd, NULL, NULL);
  if (client_fd < 0) {
    perror("server: accept");
    close(fd);
    exit(1);
  }

  connections[user_index].sock_fd = client_fd;
  connections[user_index].username = NULL;
  return client_fd;
}

int main(int argc, char ** argv) {

  if (argc >= 3) {
    fprintf(stderr, "to many arguments");
    exit(1);
  }

  int n;
  if (argc == 2) {
    char * next = NULL;
    n = strtol(argv[1], & next, 10);
  }

  struct sockname connections[MAX_CONNECTIONS];
  for (int index = 0; index < MAX_CONNECTIONS; index++) {
    connections[index].sock_fd = -1;
    connections[index].username = NULL;
  }

  // Create the socket FD.
  int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    perror("server: socket");
    exit(1);
  }

  // Set information about the port (and IP) we want to be connected to.
  struct sockaddr_in server;
  server.sin_family = AF_INET;

  if (argc == 1) {
    server.sin_port = htons(60000);
  }

  if (argc == 2) {
    server.sin_port = htons(60000 + n);
  }

  server.sin_addr.s_addr = INADDR_ANY;

  // This should always be zero. On some systems, it won't error if you
  // forget, but on others, you'll get mysterious errors. So zero it.
  memset( & server.sin_zero, 0, 8);

  int flag = 0; // variable to see if first port failed
  // Bind the selected port to the socket.
  if (bind(sock_fd, (struct sockaddr * ) & server, sizeof(server)) < 0) {
    flag = 1;
    if (argc == 1) {
      perror("server could not bind to port 60000, will try to bind to port 60001: bind");
    }

    if (argc == 2) {
      perror("server could not bind to port 60000 + input, will try to bind to port 60001 + input: bind");
    }

  }

  if (flag == 1) { // first port failed to bind lets try to bind to the second one

    if (argc == 1) {
      server.sin_port = htons(60001);

      if (bind(sock_fd, (struct sockaddr * ) & server, sizeof(server)) < 0) {

        perror("server could not bind to port 60001, will exit: bind");
        close(sock_fd);
        exit(1);
      }

    }

    if (argc == 2) {
      server.sin_port = htons(60001 + n);

      if (bind(sock_fd, (struct sockaddr * ) & server, sizeof(server)) < 0) {

        perror("server could not bind to port 60001 + input, will exit: bind");
        close(sock_fd);
        exit(1);
      }

    }

  }

  // Announce willingness to accept connections on this socket.
  if (listen(sock_fd, MAX_BACKLOG) < 0) {
    perror("server: listen");
    close(sock_fd);
    exit(1);
  }

  // The client accept - message accept loop. First, we prepare to listen
  // to multiple file descriptors by initializing a set of file descriptors.
  int max_fd = sock_fd;
  fd_set all_fds;
  FD_ZERO( & all_fds);
  FD_SET(sock_fd, & all_fds);

  int players = 0; // keep track of the number of players we have to see if we can create game after current game

  // variables for player 1
  char buf_p1[BUF_SIZE] = {
    '\0'
  };
  int inbuf_p1 = 0; // How many bytes currently in buffer?
  int room_p1 = sizeof(buf_p1); // How many bytes remaining in buffer?
  char * after_p1 = buf_p1; // Pointer to position after the data in buf
  int nbytes_p1;
  int procedure_p1 = 0;
  char gesture_p1[1000];
  int p1_index;
  int p1_points = 0;

  // variables for player 2
  char buf_p2[BUF_SIZE] = {
    '\0'
  };
  int inbuf_p2 = 0; // How many bytes currently in buffer?
  int room_p2 = sizeof(buf_p2); // How many bytes remaining in buffer?
  char * after_p2 = buf_p2; // Pointer to position after the data in buf
  int nbytes_p2;
  int procedure_p2 = 0;
  char gesture_p2[1000];
  int p2_index;
  int p2_points = 0;

  // variables for setting up game
  int game_over = 0;
  int player_num = 0;
  int games_ready = 0;
  int game_in_progress = 0;
  int games_played = 0;

  while (1) {

    if ((games_ready > 0) && (game_in_progress == 0)) {

      player_num++;
      p1_index = player_num;
      write(connections[p1_index].sock_fd, "enter username\r\n", 16);
      player_num++;
      p2_index = player_num;
      write(connections[p2_index].sock_fd, "enter username\r\n", 16);

      games_ready -= 1;
      game_in_progress = 1;
    }

    // select updates the fd_set it receives, so we always use a copy and retain the original.
    fd_set listen_fds = all_fds;
    int nready = select(max_fd + 1, & listen_fds, NULL, NULL, NULL);
    if (nready == -1) {
      perror("server: select");
      exit(1);
    }

    // Is it the original socket? Create a new connection ...
    if (FD_ISSET(sock_fd, & listen_fds)) {
      int client_fd = accept_connection(sock_fd, connections);
      if (client_fd > max_fd) {
        max_fd = client_fd;
      }
      FD_SET(client_fd, & all_fds);
 //     printf("Accepted connection\n");                     //uncomment if you want to see if people connect

      players++;
      if (players == 2) { // when we have 2 connections send message to players saying enter username to play gaeme

        p1_index = player_num;
        player_num++;
        p2_index = player_num;
        write(connections[p1_index].sock_fd, "enter username\r\n", 16);
        write(connections[p2_index].sock_fd, "enter username\r\n", 16);
        game_in_progress = 1;

      }

      if ((players > 2) && (players % 2 == 0)) { // essenitally keep track of ammount of people connected to see how much games we can set up
        games_ready++;
      }

    }

    // Next, check the clients.
    // NOTE: We could do some tricks with nready to terminate this loop early.
    for (int index = 0; index < MAX_CONNECTIONS; index++) {
      if (connections[index].sock_fd > -1 &&
        FD_ISSET(connections[index].sock_fd, & listen_fds)) {
        // Note: never reduces max_fd

        if (index == p1_index) {

          while ((nbytes_p1 = read(connections[index].sock_fd, after_p1, room_p1)) > 0) {
            // Step 1: update inbuf (how many bytes were just added?)
            inbuf_p1 += nbytes_p1;

            int where;

            // Step 2: the loop condition below calls find_network_newline
            // to determine if a full line has been read from the client.
            // Your next task should be to implement find_network_newline
            // (found at the bottom of this file).
            //
            // Note: we use a loop here because a single read might result in
            // more than one full line.
            while ((where = find_network_newline(buf_p1, inbuf_p1)) > 0) {
              // Step 3: Okay, we have a full line.
              // Output the full line, not including the "\r\n",
              // using print statement below.
              // Be sure to put a '\0' in the correct place first;
              // otherwise you'll get junk in the output.

              buf_p1[where - 2] = '\0';

              if (procedure_p1 == 0) { //first procedure get username
                connections[index].username = buf_p1;
              }

              if (procedure_p1 == 1) { //second procedure get gesture
                strncpy(gesture_p1, buf_p1, 2);
              }

              // Note that we could have also used write to avoid having to
              // put the '\0' in the buffer. Try using write later!

              // Step 4: update inbuf and remove the full line from the buffer
              // There might be stuff after the line, so don't just do inbuf = 0.

              inbuf_p1 -= where;

              // You want to move the stuff after the full line to the beginning
              // of the buffer.  A loop can do it, or you can use memmove.
              // memmove(destination, source, number_of_bytes)

              memmove(buf_p1, buf_p1 + where, where);
            }
            // Step 5: update after and room, in preparation for the next read.

            after_p1 = buf_p1 + inbuf_p1;
            room_p1 = BUF_SIZE - inbuf_p1;

            if (procedure_p1 == 0) {
              write(connections[index].sock_fd, "enter gesture\r\n", 15);
            }

            procedure_p1++;
            break;

          }

        }

        if (index == p2_index) {

          while ((nbytes_p2 = read(connections[index].sock_fd, after_p2, room_p2)) > 0) {
            // Step 1: update inbuf (how many bytes were just added?)
            inbuf_p2 += nbytes_p2;

            int where;

            // Step 2: the loop condition below calls find_network_newline
            // to determine if a full line has been read from the client.
            // Your next task should be to implement find_network_newline
            // (found at the bottom of this file).
            //
            // Note: we use a loop here because a single read might result in
            // more than one full line.
            while ((where = find_network_newline(buf_p2, inbuf_p2)) > 0) {
              // Step 3: Okay, we have a full line.
              // Output the full line, not including the "\r\n",
              // using print statement below.
              // Be sure to put a '\0' in the correct place first;
              // otherwise you'll get junk in the output.

              buf_p2[where - 2] = '\0';

              if (procedure_p2 == 0) { //first procedure get username
                connections[index].username = buf_p2;
              }

              if (procedure_p2 == 1) { //second procedure get gesture
                strncpy(gesture_p2, buf_p2, 2);
              }

              // Note that we could have also used write to avoid having to
              // put the '\0' in the buffer. Try using write later!

              // Step 4: update inbuf and remove the full line from the buffer
              // There might be stuff after the line, so don't just do inbuf = 0.

              inbuf_p2 -= where;

              // You want to move the stuff after the full line to the beginning
              // of the buffer.  A loop can do it, or you can use memmove.
              // memmove(destination, source, number_of_bytes)

              memmove(buf_p2, buf_p2 + where, where);
            }

            // Step 5: update after and room, in preparation for the next read.

            after_p2 = buf_p2 + inbuf_p2;
            room_p2 = BUF_SIZE - inbuf_p2;

            if (procedure_p2 == 0) {
              write(connections[index].sock_fd, "enter gesture\r\n", 15);
            }

            procedure_p2++;
            break;
          }

        }

        if (procedure_p1 == 2 && procedure_p2 == 2) { // both p1 and p2 have written their gestures, determine who wins  write enter gesture and set procedure to 1

          if ((gesture_p1[0] == 'e') || (gesture_p2[0] == 'e')) {

            char games_played_buf[10];
            sprintf(games_played_buf, "%d", games_played);
            int holder = strlen(games_played_buf);
            games_played_buf[holder] = '\r';
            games_played_buf[holder + 1] = '\n';

            write(connections[p1_index].sock_fd, games_played_buf, holder + 2); // inform player of number of games played
            write(connections[p2_index].sock_fd, games_played_buf, holder + 2); // inform player of number of games played

            char games_p1_points[10];
            char games_p2_points[10];
            sprintf(games_p1_points, "%d", p1_points);
            sprintf(games_p2_points, "%d", p2_points);
            int p1_holder = strlen(games_p1_points);
            int p2_holder = strlen(games_p2_points);
            games_p1_points[p1_holder] = '\r';
            games_p1_points[p1_holder + 1] = '\n';
            games_p2_points[p2_holder] = '\r';
            games_p2_points[p2_holder + 1] = '\n';

            write(connections[p1_index].sock_fd, games_p1_points, p1_holder + 2); //games won
            write(connections[p2_index].sock_fd, games_p2_points, p2_holder + 2); //games won

            game_over = 1;

            close(connections[p1_index].sock_fd);
            close(connections[p2_index].sock_fd);
            FD_CLR(connections[p1_index].sock_fd, & all_fds);
            FD_CLR(connections[p2_index].sock_fd, & all_fds);

          } else if ((gesture_p1[0] == 'p') && ((gesture_p2[0] == 'r') || (gesture_p2[0] == 'S'))) {

            write(connections[p1_index].sock_fd, "win\r\n", 5); 
            write(connections[p2_index].sock_fd, "lose\r\n", 6); 
            p1_points++;

          } else if ((gesture_p1[0] == 'r') && ((gesture_p2[0] == 's') || (gesture_p2[0] == 'l'))) {

            write(connections[p1_index].sock_fd, "win\r\n", 5);
            write(connections[p2_index].sock_fd, "lose\r\n", 6);
            p1_points++;

          } else if ((gesture_p1[0] == 's') && ((gesture_p2[0] == 'l') || (gesture_p2[0] == 'p'))) {

            write(connections[p1_index].sock_fd, "win\r\n", 5);
            write(connections[p2_index].sock_fd, "lose\r\n", 6);
            p1_points++;

          } else if ((gesture_p1[0] == 'S') && ((gesture_p2[0] == 'r') || (gesture_p2[0] == 's'))) {

            write(connections[p1_index].sock_fd, "win\r\n", 5);
            write(connections[p2_index].sock_fd, "lose\r\n", 6);
            p1_points++;

          } else if ((gesture_p1[0] == 'l') && ((gesture_p2[0] == 'S') || (gesture_p2[0] == 'p'))) {

            write(connections[p1_index].sock_fd, "win\r\n", 5);
            write(connections[p2_index].sock_fd, "lose\r\n", 6);
            p1_points++;

          }
          //----------------------------------------------------------all potential points for cli have been calculated
          else if ((gesture_p2[0] == 'p') && ((gesture_p1[0] == 'r') || (gesture_p1[0] == 'S'))) {

            write(connections[p2_index].sock_fd, "win\r\n", 5);
            write(connections[p1_index].sock_fd, "lose\r\n", 6);
            p2_points++;

          } else if ((gesture_p2[0] == 'r') && ((gesture_p1[0] == 's') || (gesture_p1[0] == 'l'))) {

            write(connections[p2_index].sock_fd, "win\r\n", 5);
            write(connections[p1_index].sock_fd, "lose\r\n", 6);
            p2_points++;

          } else if ((gesture_p2[0] == 's') && ((gesture_p1[0] == 'l') || (gesture_p1[0] == 'p'))) {

            write(connections[p2_index].sock_fd, "win\r\n", 5);
            write(connections[p1_index].sock_fd, "lose\r\n", 6);
            p2_points++;

          } else if ((gesture_p2[0] == 'S') && ((gesture_p1[0] == 'r') || (gesture_p1[0] == 's'))) {

            write(connections[p2_index].sock_fd, "win\r\n", 5);
            write(connections[p1_index].sock_fd, "lose\r\n", 6);
            p2_points++;

          } else if ((gesture_p2[0] == 'l') && ((gesture_p1[0] == 'S') || (gesture_p1[0] == 'p'))) {

            write(connections[p2_index].sock_fd, "win\r\n", 5);
            write(connections[p1_index].sock_fd, "lose\r\n", 6);
            p2_points++;

          } else if ((gesture_p1[0] == gesture_p2[0])) {

            write(connections[p2_index].sock_fd, "tie\r\n", 5);
            write(connections[p1_index].sock_fd, "tie\r\n", 5);

          }

          if (game_over == 1) { // reset variables in preperation for a game between new players

            buf_p1[BUF_SIZE] = '\0';
            inbuf_p1 = 0; // How many bytes currently in buffer?
            room_p1 = sizeof(buf_p1); // How many bytes remaining in buffer?
            after_p1 = buf_p1; // Pointer to position after the data in buf
            procedure_p1 = 0; 
            memset(gesture_p1, '\0', 1000);
            p1_points = 0;

            buf_p2[BUF_SIZE] = '\0';
            inbuf_p2 = 0; // How many bytes currently in buffer?
            room_p2 = sizeof(buf_p2); // How many bytes remaining in buffer?
            after_p2 = buf_p2; // Pointer to position after the data in buf
            procedure_p2 = 0; //i defined
            memset(gesture_p2, '\0', 1000);
            p2_points = 0;

            game_over = 0;
            game_in_progress = 0;
            games_played = 0;

          }else{            // continue with current game

            memset(gesture_p1, '\0', 1000);
            memset(gesture_p2, '\0', 1000);

            write(connections[p1_index].sock_fd, "enter gesture\r\n", 15);
            write(connections[p2_index].sock_fd, "enter gesture\r\n", 15);
            procedure_p1 = 1;
            procedure_p2 = 1;
            games_played++;

          }
        }

      }
    }
  }

  // Should never get here.
  return 1;
}