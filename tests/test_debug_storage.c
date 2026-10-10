#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "bt_debug_storage.h"

int main(int argc, char **argv)
{
    assert(argc == 2);
    bt_debug_storage_t storage;
    bt_debug_storage_init(&storage, 16, 2);
    assert(!bt_debug_storage_open(&storage, false, "/no-sd", 1));
    assert(storage.state == BT_DEBUG_SD_NO_CARD);
    assert(!bt_debug_storage_write(&storage, "abc", 3));
    assert(!bt_debug_storage_open(&storage, true, "/no-sd", 1));
    assert(storage.state == BT_DEBUG_SD_ERROR);
    assert(bt_debug_storage_open(&storage, true, argv[1], 1));
    assert(storage.state == BT_DEBUG_SD_RECORDING);
    char first_path[sizeof(storage.path)];
    strcpy(first_path, storage.path);
    assert(bt_debug_storage_write(&storage, "1234567890", 10));
    assert(bt_debug_storage_write(&storage, "abcdefghij", 10));
    assert(storage.file_index == 1 && storage.bytes_written == 20);
    assert(!bt_debug_storage_write(&storage, "1234567890", 10));
    assert(storage.state == BT_DEBUG_SD_LIMIT && storage.file == NULL);
    bt_debug_storage_close(&storage);
    assert(storage.state == BT_DEBUG_SD_OFF);
    FILE *file = fopen(first_path, "rb");
    assert(file);
    char data[32] = {0};
    assert(fread(data, 1, sizeof(data), file) == 10);
    assert(strcmp(data, "1234567890") == 0);
    fclose(file);
    assert(bt_debug_storage_open(&storage, true, argv[1], 1)); /* collision must not overwrite */
    assert(strcmp(first_path, storage.path) != 0);
    assert(bt_debug_storage_write(&storage, "raw", 3));
    assert(bt_debug_storage_flush(&storage));
    assert(!bt_debug_storage_write(&storage, "this is larger than one file", 27));
    assert(storage.state == BT_DEBUG_SD_ERROR);
    assert(!bt_debug_storage_flush(&storage));
    bt_debug_storage_close(&storage);
    bt_debug_storage_close(NULL);
    assert(!bt_debug_storage_open(NULL, true, argv[1], 1));
    assert(!bt_debug_storage_write(NULL, "a", 1));
    assert(!bt_debug_storage_flush(NULL));
    bt_debug_storage_init(&storage, 16, 2);
    assert(!bt_debug_storage_open(&storage, true, NULL, 1));
    assert(bt_debug_storage_open(&storage, true, argv[1], 2));
    assert(!bt_debug_storage_write(&storage, NULL, 1));
    bt_debug_storage_close(&storage);
    /* Host fault injection: a failing file sink must close and expose an error. */
    storage.file = fopen("/dev/full", "wb");
    assert(storage.file);
    setvbuf(storage.file, NULL, _IONBF, 0);
    storage.state = BT_DEBUG_SD_RECORDING;
    assert(!bt_debug_storage_write(&storage, "fail", 4));
    assert(storage.state == BT_DEBUG_SD_ERROR && storage.last_errno != 0 && storage.file == NULL);
    /* A successful stdio flush alone must not hide a failed filesystem sync. */
    int pipe_fds[2];
    assert(pipe(pipe_fds) == 0);
    storage.file = fdopen(pipe_fds[1], "wb");
    assert(storage.file);
    storage.state = BT_DEBUG_SD_RECORDING;
    assert(!bt_debug_storage_flush(&storage));
    assert(storage.state == BT_DEBUG_SD_ERROR && storage.file == NULL);
    close(pipe_fds[0]);
    puts("debug storage: no-card, append, rotation, limit, no-overwrite and IO failure PASS");
}
