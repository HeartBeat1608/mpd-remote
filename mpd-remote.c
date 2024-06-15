#include <mpd/albumart.h>
#include <mpd/client.h>
#include <mpd/player.h>
#include <mpd/queue.h>
#include <mpd/song.h>
#include <stdio.h>
#include <string.h>

const char *REMOTE_COMMAND_ADD = "add";
const char *REMOTE_COMMAND_STOP = "stop";
const char *REMOTE_COMMAND_NEXT = "next";
const char *REMOTE_COMMAND_PAUSE = "pause";
const char *REMOTE_COMMAND_PLAY_RESUME = "play";
const char *REMOTE_COMMAND_LIST = "list";
const char *REMOTE_COMMAND_CLEAR = "clear";

int cleanup(struct mpd_connection *conn) {
  mpd_connection_free(conn);
  return 1;
}

unsigned int check_command(const char *cmd) {
  if (!strcmp(cmd, REMOTE_COMMAND_PAUSE))
    return 1;
  if (!strcmp(cmd, REMOTE_COMMAND_STOP))
    return 2;
  if (!strcmp(cmd, REMOTE_COMMAND_PLAY_RESUME))
    return 3;
  if (!strcmp(cmd, REMOTE_COMMAND_NEXT))
    return 4;
  if (!strcmp(cmd, REMOTE_COMMAND_ADD))
    return 5;
  if (!strcmp(cmd, REMOTE_COMMAND_LIST))
    return 6;
  if (!strcmp(cmd, REMOTE_COMMAND_CLEAR))
    return 7;
  return 0;
}

struct mpd_connection *connect_mpd() {
  printf("[LOG] Initializing mpd connection\n");
  struct mpd_connection *conn = mpd_connection_new(NULL, 0, 0);
  if (conn == NULL) {
    fprintf(stderr, "[ERROR] Out of Memory\n");
    return NULL;
  }

  if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
    fprintf(stderr, "[ERROR] %s\n", mpd_connection_get_error_message(conn));
    mpd_connection_free(conn);
    return NULL;
  }
  printf("[LOG] Connected to MPD instance\n");
  return conn;
}

bool clear_current_queue(struct mpd_connection* conn) {
  printf("[REMOTE] Loading Current Queue\n");
  return mpd_send_clear(conn);
}

bool list_mpd_playlist(struct mpd_connection *conn) {
  printf("[REMOTE] Loading Current Queue\n");
  if (!mpd_send_list_queue_meta(conn)) {
    fprintf(stderr, "[REMOTE] Could not send queue list command\n");
    return false;
  }

  unsigned int count = 0;
  while (true) {
    struct mpd_entity *ent = mpd_recv_entity(conn);
    if (ent == NULL) {
      break;
    }

    const struct mpd_song *song = mpd_entity_get_song(ent);
    const char *song_uri = mpd_song_get_uri(song);
    unsigned int song_id = mpd_song_get_id(song);
    count++;

    printf("[REMOTE] (%d) Song %s\n", song_id, song_uri);
  }

  if (!count) {
    printf("[REMOTE] No Songs in the current queue\n");
  }

  return true;
}

bool play_resume_mpd(struct mpd_connection *conn) {
  struct mpd_status *status = mpd_run_status(conn);
  enum mpd_state state = mpd_status_get_state(status);

  struct mpd_song *current_song = mpd_run_current_song(conn);

  switch (state) {
  case MPD_STATE_PLAY:
    printf("[REMOTE] MPD is already playing\n");
    return true;
  case MPD_STATE_PAUSE:
    printf("[REMOTE] MPD is currently paused; Resuming...\n");
    return mpd_send_pause(conn, false);
  case MPD_STATE_STOP:
    printf("[REMOTE] MPD is currently stopped; Starting...\n");
    if (current_song == NULL) {
      printf("[REMOTE] MPD has not song loaded; Moving to next song in the queue...\n");
      return mpd_run_next(conn);
    } else {
      const char* uri = mpd_song_get_uri(current_song);
      printf("[REMOTE] Current Song: %s\n", uri);
      return mpd_send_play(conn);
    }
  case MPD_STATE_UNKNOWN:
    fprintf(stderr, "[REMOTE] Could not load MPD state. Check if your mpd "
                    "instance is running\n");
    return false;
  }
}

bool add_mpd(struct mpd_connection *conn, const char *uri) {
  printf("[REMOTE] Adding Song %s\n", uri);

  bool status = mpd_send_add(conn, uri);
  if (!status) {
    fprintf(stderr, "[REMOTE] Failed to add uri to mpd\n");
    return cleanup(conn);
  }

  return true;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: mpd-remote [play|pause|next|list|add|clear|stop]");
    return 1;
  }

  struct mpd_connection *conn = connect_mpd();
  char *command = argv[1];

  unsigned int command_id = check_command(command);
  if (!command_id) {
    fprintf(stderr, "[REMOTE] Invalid command %s\n", command);
    return cleanup(conn);
  }

  bool status;
  switch (command_id) {
  case 1:
    status = mpd_send_pause(conn, true);
    break;
  case 2:
    status = mpd_send_stop(conn);
    break;
  case 3:
    status = play_resume_mpd(conn);
    break;
  case 4:
    status = mpd_send_next(conn);
    break;
  case 5:
    if (argc <= 2) {
      fprintf(stderr, "Usage: mpd-remote add <url of song>");
      return cleanup(conn);
    }
    char *uri = argv[2];
    status = add_mpd(conn, uri);
    break;
  case 6:
    status = list_mpd_playlist(conn);
    break;
  case 7:
    status = clear_current_queue(conn);
    break;
  default:
    status = false;
    break;
  }

  int x = cleanup(conn);
  return status && !x ? 0 : 1;
}
