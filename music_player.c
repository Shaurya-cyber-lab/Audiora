#include "music_player.h"
#include <pthread.h>
#include <time.h>

// Platform-specific audio headers
#ifdef _WIN32
    #include <windows.h>
    #include <mmsystem.h>
    #include <unistd.h>
    #pragma comment(lib, "winmm.lib")
    
#elif __APPLE__
    #include <AudioToolbox/AudioToolbox.h>
    #include <unistd.h>
#elif __linux__
    #include <unistd.h>
#endif

int isPlaying = 0; // Global variable to track audio state
int autoPlayEnabled = 1; // NEW: Global flag for auto-play feature
int manualStop = 0; // NEW: Flag to indicate user manually stopped playback
pthread_t playbackThread; // NEW: Thread for monitoring playback
int stopPlaybackThread = 0; // NEW: Flag to stop the monitoring thread
MusicPlayer* globalPlayer = NULL; // NEW: Global player reference for thread
pthread_mutex_t playbackMutex = PTHREAD_MUTEX_INITIALIZER; // NEW: Mutex for thread safety
time_t songStartTime = 0; // NEW: Track when current song started
int currentSongDuration = 0; // NEW: Store duration of current song

// ============================================================================
// AUTO-PLAY MONITORING THREAD (NEW)
// ============================================================================

// NEW: Thread function - simple time-based approach
void* monitorPlaybackThread(void* arg) {
    MusicPlayer* player = (MusicPlayer*)arg;
    int checkInterval = 2; // Check every 2 seconds

    while (!stopPlaybackThread) {
        sleep(checkInterval);

        pthread_mutex_lock(&playbackMutex);
        
        // NEW: Don't auto-play if user manually stopped
        if (autoPlayEnabled && isPlaying && !manualStop && player->currentSong && currentSongDuration > 0) {
            time_t elapsed = time(NULL) - songStartTime;
            
            // If elapsed time exceeds song duration + 1 second buffer, play next
            if (elapsed >= (currentSongDuration + 1)) {
                printf("\n[Auto-Play Monitor] Song finished! Playing next...\n");
                isPlaying = 0; // Reset flag before calling playNext
                pthread_mutex_unlock(&playbackMutex);
                playNext(player);
                pthread_mutex_lock(&playbackMutex);
            }
        }

        pthread_mutex_unlock(&playbackMutex);
    }

    return NULL;
}

// ============================================================================
// AUDIO PLAYBACK FUNCTIONS (Platform-Specific)
// ============================================================================

#ifdef _WIN32
void playAudioWindows(const char* filepath) {
    char command[512];
    mciSendString("close mp3", NULL, 0, NULL);
    sprintf(command, "open \"%s\" type mpegvideo alias mp3", filepath);
    if (mciSendString(command, NULL, 0, NULL) != 0) {
        printf("Error: Could not open audio file.\n");
        return;
    }
    if (mciSendString("play mp3", NULL, 0, NULL) != 0) {
        printf("Error: Could not play audio file.\n");
        mciSendString("close mp3", NULL, 0, NULL);
        return;
    }
    isPlaying = 1;
    printf("♪ Audio playback started!\n");
}

void stopAudioWindows() {
    mciSendString("stop mp3", NULL, 0, NULL);
    mciSendString("close mp3", NULL, 0, NULL);
    isPlaying = 0;
}

int isAudioPlayingWindows() {
    char status[128];
    mciSendString("status mp3 mode", status, sizeof(status), NULL);
    return (strcmp(status, "playing") == 0);
}
#elif __APPLE__
void playAudioMac(const char* filepath) {
    char command[512];
    sprintf(command, "afplay \"%s\" &", filepath);
    system(command);
    isPlaying = 1;
    printf("♪ Audio playback started!\n");
}

void stopAudioMac() {
    system("killall afplay 2>/dev/null");
    isPlaying = 0;
}
#elif __linux__
void playAudioLinux(const char* filepath) {
    char command[512];
    if (system("which mpg123 > /dev/null 2>&1") == 0)
        sprintf(command, "mpg123 -q \"%s\" &", filepath);
    else if (system("which ffplay > /dev/null 2>&1") == 0)
        sprintf(command, "ffplay -nodisp -autoexit \"%s\" &", filepath);
    else if (system("which aplay > /dev/null 2>&1") == 0)
        sprintf(command, "aplay \"%s\" &", filepath);
    else {
        printf("Error: No audio player found.\n");
        return;
    }
    system(command);
    isPlaying = 1;
    printf("♪ Audio playback started!\n");
}

void stopAudioLinux() {
    system("killall mpg123 2>/dev/null");
    system("killall ffplay 2>/dev/null");
    system("killall aplay 2>/dev/null");
    isPlaying = 0;
}
#endif

// ============================================================================
// CROSS PLATFORM AUDIO INTERFACE
// ============================================================================
void playAudioFile(const char* filepath) {
#ifdef _WIN32
    playAudioWindows(filepath);
#elif __APPLE__
    playAudioMac(filepath);
#elif __linux__
    playAudioLinux(filepath);
#else
    printf("Audio not supported.\n");
#endif
}

void stopAudioFile() {
#ifdef _WIN32
    stopAudioWindows();
#elif __APPLE__
    stopAudioMac();
#elif __linux__
    stopAudioLinux();
#endif
    
    // NEW: Reset auto-play tracking variables when stopping
    isPlaying = 0;
    songStartTime = 0;
    currentSongDuration = 0;
}

int isAudioPlaying() {
#ifdef _WIN32
    return isAudioPlayingWindows();
#else
    return isPlaying;
#endif
}

// ============================================================================
// INITIALIZATION AND CLEANUP
// ============================================================================
MusicPlayer* initMusicPlayer() {
    MusicPlayer* player = (MusicPlayer*)malloc(sizeof(MusicPlayer));
    if (!player) exit(1);

    player->playlist = NULL;
    player->recentlyPlayed = NULL;
    player->currentSong = NULL;
    player->songCount = 0;
    player->nextId = 1;

    player->upcomingQueue = (Queue*)malloc(sizeof(Queue));
    player->upcomingQueue->front = player->upcomingQueue->rear = NULL;
    player->upcomingQueue->count = 0;

    // NEW: Set global player reference and start monitoring thread
    globalPlayer = player;
    stopPlaybackThread = 0;
    pthread_create(&playbackThread, NULL, monitorPlaybackThread, player);

    return player;
}

void freeMusicPlayer(MusicPlayer* player) {
    if (!player) return;
    
    // NEW: Stop the playback monitoring thread
    stopPlaybackThread = 1;
    pthread_join(playbackThread, NULL);
    pthread_mutex_destroy(&playbackMutex);

    stopAudioFile();

    Song* current = player->playlist;
    while (current) {
        Song* temp = current;
        current = current->next;
        free(temp);
    }

    clearRecentlyPlayed(player);
    clearUpcoming(player);
    free(player->upcomingQueue);
    free(player);
    globalPlayer = NULL;
}

// ============================================================================
// PLAYLIST MANAGEMENT
// ============================================================================
void addSong(MusicPlayer* player, const char* title, const char* artist, int duration, const char* filepath) {
    Song* newSong = (Song*)malloc(sizeof(Song));
    if (!newSong) return;

    newSong->id = player->nextId++;
    strncpy(newSong->title, title, MAX_TITLE);
    strncpy(newSong->artist, artist, MAX_ARTIST);
    newSong->duration = duration;
    strncpy(newSong->filepath, filepath, MAX_FILENAME);
    newSong->next = NULL;

    if (!player->playlist) player->playlist = newSong;
    else {
        Song* current = player->playlist;
        while (current->next) current = current->next;
        current->next = newSong;
    }

    player->songCount++;
    printf("\n✓ Song added successfully! (ID: %d)\n", newSong->id);`
}

void deleteSong(MusicPlayer* player, int songId) {
    if (!player->playlist) return;
    Song* current = player->playlist;
    Song* prev = NULL;

    while (current) {
        if (current->id == songId) {
            if (prev) prev->next = current->next;
            else player->playlist = current->next;
            free(current);
            player->songCount--;
            printf("\n✓ Song deleted.\n");
            return;
        }
        prev = current;
        current = current->next;
    }
    printf("\n✗ Song ID not found.\n");
}

Song* findSongById(MusicPlayer* player, int songId) {
    Song* current = player->playlist;
    while (current) {
        if (current->id == songId) return current;
        current = current->next;
    }
    return NULL;
}

// NEW: Find the next song in playlist after current song
Song* findNextSong(MusicPlayer* player, Song* currentSong) {
    if (!currentSong || !player->playlist) return NULL;
    
    Song* current = player->playlist;
    while (current) {
        if (current == currentSong && current->next) {
            return current->next;
        }
        current = current->next;
    }
    return NULL;
}

// ============================================================================
// PLAYBACK OPERATIONS
// ============================================================================
void playSong(MusicPlayer* player, int songId) {
    Song* song = findSongById(player, songId);
    if (!song) {
        printf("\nSong not found!\n");
        return;
    }

    pthread_mutex_lock(&playbackMutex);
    
    if (isAudioPlaying()) {
        stopAudioFile();
    }
    if (player->currentSong) {
        pushToRecentlyPlayed(player, player->currentSong);
    }
    player->currentSong = song;
    
    // NEW: Reset manual stop flag when playing new song
    manualStop = 0;
    
    // NEW: Record song start time and duration for auto-play tracking
    songStartTime = time(NULL);
    currentSongDuration = song->duration;

    printf("\nNow Playing: %s - %s (%d sec)\n", song->artist, song->title, song->duration);
    if (strlen(song->filepath) > 0) {
        playAudioFile(song->filepath);
    } else {
        printf("(No audio file associated)\n");
        isPlaying = 0;
    }

    pthread_mutex_unlock(&playbackMutex);
}

// NEW: Play the next song in the playlist (called by monitoring thread)
void playNext(MusicPlayer* player) {
    pthread_mutex_lock(&playbackMutex);
    
    if (!player->currentSong) {
        printf("\nNo song currently playing.\n");b
        pthread_mutex_unlock(&playbackMutex);
        return;
    }

    Song* nextSong = findNextSong(player, player->currentSong);
    
    if (!nextSong) {
        printf("\n♪ Playlist finished! No more songs to play.\n");
        isPlaying = 0;
        pthread_mutex_unlock(&playbackMutex);
        return;
    }

    printf("\n⏭  Auto-playing next: %s - %s (%d sec)\n", nextSong->artist, nextSong->title, nextSong->duration);
    
    // NEW: Check if filepath exists before trying to play
    if (strlen(nextSong->filepath) == 0) {
        printf("ERROR: Next song has no audio file!\n");
        player->currentSong = nextSong;
        isPlaying = 0;
        pthread_mutex_unlock(&playbackMutex);
        return;
    }
    
    // NEW: Update player state BEFORE releasing mutex
    if (player->currentSong) {
        pushToRecentlyPlayed(player, player->currentSong);
    }
    player->currentSong = nextSong;
    songStartTime = time(NULL);
    currentSongDuration = nextSong->duration;
    
    printf("[DEBUG] About to call playAudioFile for: %s\n", nextSong->filepath);
    
    // NEW: Release mutex BEFORE calling audio functions
    pthread_mutex_unlock(&playbackMutex);
    
    // NEW: Call these functions OUTSIDE of mutex lock
    stopAudioFile();
    sleep(1); // NEW: Small delay to ensure clean stop
    playAudioFile(nextSong->filepath);
    printf("[DEBUG] playAudioFile completed, isPlaying = %d\n", isPlaying);
}

// NEW: Toggle auto-play feature on/off
void toggleAutoPlay(MusicPlayer* player) {
    autoPlayEnabled = !autoPlayEnabled;
    printf("\nAuto-play is now %s.\n", autoPlayEnabled ? "ENABLED" : "DISABLED");
}

// ============================================================================
// STACK & QUEUE (Simple Versions)
// ============================================================================
void pushToRecentlyPlayed(MusicPlayer* player, Song* song) {
    StackNode* node = (StackNode*)malloc(sizeof(StackNode));
    node->song = song;
    node->next = player->recentlyPlayed;
    player->recentlyPlayed = node;
}

void clearRecentlyPlayed(MusicPlayer* player) {
    StackNode* temp;
    while (player->recentlyPlayed) {
        temp = player->recentlyPlayed;
        player->recentlyPlayed = player->recentlyPlayed->next;
        free(temp);
    }
}

void enqueueUpcoming(MusicPlayer* player, Song* song) {
    QueueNode* node = (QueueNode*)malloc(sizeof(QueueNode));
    node->song = song;
    node->next = NULL;

    if (!player->upcomingQueue->rear)
        player->upcomingQueue->front = player->upcomingQueue->rear = node;
    else {
        player->upcomingQueue->rear->next = node;
        player->upcomingQueue->rear = node;
    }
    player->upcomingQueue->count++;
}

Song* dequeueUpcoming(MusicPlayer* player) {
    if (!player->upcomingQueue->front) return NULL;
    QueueNode* temp = player->upcomingQueue->front;
    Song* song = temp->song;
    player->upcomingQueue->front = temp->next;
    if (!player->upcomingQueue->front) player->upcomingQueue->rear = NULL;
    free(temp);
    player->upcomingQueue->count--;
    return song;
}

void clearUpcoming(MusicPlayer* player) {
    while (dequeueUpcoming(player));
}

// ============================================================================
// FILE HANDLING
// ============================================================================
void savePlaylistToFile(MusicPlayer* player, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return;

    fprintf(file, "%d\n", player->nextId);
    Song* current = player->playlist;
    while (current) {
        fprintf(file, "%d|%s|%s|%d|%s\n",
                current->id, current->title, current->artist,
                current->duration, current->filepath);
        current = current->next;
    }
    fclose(file);
}

void loadPlaylistFromFile(MusicPlayer* player, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return;
    fscanf(file, "%d\n", &player->nextId);

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        Song* newSong = (Song*)malloc(sizeof(Song));
        sscanf(line, "%d|%99[^|]|%99[^|]|%d|%149[^\n]",
               &newSong->id, newSong->title, newSong->artist,
               &newSong->duration, newSong->filepath);
        newSong->next = NULL;

        if (!player->playlist) player->playlist = newSong;
        else {
            Song* curr = player->playlist;
            while (curr->next) curr = curr->next;
            curr->next = newSong;
        }
        player->songCount++;
    }
    fclose(file);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void pauseScreen() {
    printf("\nPress Enter to continue...");
    getchar();
}

int getIntInput(const char* prompt) {
    int value;
    printf("%s", prompt);
    scanf("%d", &value);
    getchar();
    return value;
}

void getStringInput(const char* prompt, char* buffer, int maxLen) {
    printf("%s", prompt);
    fgets(buffer, maxLen, stdin);
    buffer[strcspn(buffer, "\n")] = '\0';
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================
int main() {
    MusicPlayer* player = initMusicPlayer();
    char filename[] = "playlist_audio.txt";
    int choice;

    loadPlaylistFromFile(player, filename);

    while (1) {
        clearScreen();
        printf("\n=== AUDIORA MUSIC PLAYER ===\n");
        printf("1. Add Song\n2. Delete Song\n3. Display Playlist\n4. Play Song\n");
        printf("5. Stop Playback\n6. Toggle Auto-Play\n7. Save Playlist\n8. Exit\n");

        choice = getIntInput("Enter your choice: ");
        switch (choice) {
            case 1: {
                char title[MAX_TITLE], artist[MAX_ARTIST], filepath[MAX_FILENAME];
                int duration;
                getStringInput("Title: ", title, MAX_TITLE);
                getStringInput("Artist: ", artist, MAX_ARTIST);
                duration = getIntInput("Duration (sec): ");
                getStringInput("Audio File Path: ", filepath, MAX_FILENAME);
                addSong(player, title, artist, duration, filepath);
                pauseScreen();
                break;
            }
            case 2: {
                int id = getIntInput("Enter Song ID: ");
                deleteSong(player, id);
                pauseScreen();
                break;
            }
            case 3: {
                Song* s = player->playlist;
                while (s) {
                    printf("%d | %s - %s (%d sec) [%s]\n", s->id, s->artist, s->title, s->duration,
                           strlen(s->filepath) ? "Audio ✓" : "No File");
                    s = s->next;
                }
                pauseScreen();
                break;
            }
            case 4: {
                int id = getIntInput("Enter Song ID: ");
                playSong(player, id);
                pauseScreen();
                break;
            }
            case 5:
                pthread_mutex_lock(&playbackMutex);
                manualStop = 1; // NEW: Set flag to prevent auto-play
                stopAudioFile();
                printf("\nPlayback stopped.\n");
                pthread_mutex_unlock(&playbackMutex);
                pauseScreen();
                break;
            case 6:
                toggleAutoPlay(player);
                pauseScreen();
                break;
            case 7:
                savePlaylistToFile(player, filename);
                printf("\nPlaylist saved.\n");
                pauseScreen();
                break;
            case 8:
                savePlaylistToFile(player, filename);
                freeMusicPlayer(player);
                printf("\nThanks For Using Audiora\n");
                return 0;
            default:
                printf("Invalid choice.\n");
                pauseScreen();
        }
    }
}