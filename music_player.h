#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Maximum string lengths for song attributes
#define MAX_TITLE 100
#define MAX_ARTIST 100
#define MAX_FILENAME 150

// Structure for a song node in the playlist (Linked List)
typedef struct Song {
    int id;                          // Unique song identifier
    char title[MAX_TITLE];           // Song title
    char artist[MAX_ARTIST];         // Artist name
    int duration;                    // Duration in seconds
    char filepath[MAX_FILENAME];     // Path to audio file
    struct Song* next;               // Pointer to next song in playlist
} Song;

// Structure for stack node (Recently Played)
typedef struct StackNode {
    Song* song;                      // Pointer to song
    struct StackNode* next;          // Pointer to next stack node
} StackNode;

// Structure for queue node (Upcoming Songs)
typedef struct QueueNode {
    Song* song;                      // Pointer to song
    struct QueueNode* next;          // Pointer to next queue node
} QueueNode;

// Structure for the queue (Upcoming Songs)
typedef struct Queue {
    QueueNode* front;                // Front of queue
    QueueNode* rear;                 // Rear of queue
    int count;                       // Number of songs in queue
} Queue;

// Structure for the music player system
typedef struct MusicPlayer {
    Song* playlist;                  // Head of playlist linked list
    StackNode* recentlyPlayed;       // Top of recently played stack
    Queue* upcomingQueue;            // Queue for upcoming songs
    Song* currentSong;               // Currently playing song
    int songCount;                   // Total songs in playlist
    int nextId;                      // Next available song ID
} MusicPlayer;

// Function Prototypes

// Initialization and Cleanup
MusicPlayer* initMusicPlayer();
void freeMusicPlayer(MusicPlayer* player);

// Playlist Management (Linked List Operations)
void addSong(MusicPlayer* player, const char* title, const char* artist, int duration, const char* filepath);
void deleteSong(MusicPlayer* player, int songId);
void displayPlaylist(MusicPlayer* player);
Song* findSongById(MusicPlayer* player, int songId);
int getPlaylistSize(MusicPlayer* player);

// Playback Operations
void playSong(MusicPlayer* player, int songId);
void playNext(MusicPlayer* player);
void displayCurrentSong(MusicPlayer* player);

// Stack Operations (Recently Played)
void pushToRecentlyPlayed(MusicPlayer* player, Song* song);
void displayRecentlyPlayed(MusicPlayer* player);
void clearRecentlyPlayed(MusicPlayer* player);

// Queue Operations (Upcoming Songs)
void enqueueUpcoming(MusicPlayer* player, Song* song);
Song* dequeueUpcoming(MusicPlayer* player);
void displayUpcoming(MusicPlayer* player);
void clearUpcoming(MusicPlayer* player);

// File Handling (Persistence)
void savePlaylistToFile(MusicPlayer* player, const char* filename);
void loadPlaylistFromFile(MusicPlayer* player, const char* filename);

// Utility Functions
void clearScreen();
void displayMenu();
void pauseScreen();
int getIntInput(const char* prompt);
void getStringInput(const char* prompt, char* buffer, int maxLen);

#endif // MUSIC_PLAYER_H
