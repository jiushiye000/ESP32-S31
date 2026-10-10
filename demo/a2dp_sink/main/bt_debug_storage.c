#include "bt_debug_storage.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool fail(bt_debug_storage_t *storage, bt_debug_sd_state_t state, int error)
{
    if (storage->file) {
        fclose(storage->file);
        storage->file = NULL;
    }
    storage->state = state;
    storage->last_errno = error;
    return false;
}

static bool create_file(bt_debug_storage_t *storage)
{
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        snprintf(storage->path, sizeof(storage->path), "%s/bt-%08" PRIx32 "-%02u.log",
                 storage->directory, storage->session, storage->file_index);
        int fd = open(storage->path, O_WRONLY | O_CREAT | O_EXCL, 0664);
        if (fd < 0) {
            if (errno == EEXIST) { ++storage->session; continue; }
            return fail(storage, BT_DEBUG_SD_ERROR, errno);
        }
        storage->file = fdopen(fd, "wb");
        if (!storage->file) {
            int error = errno;
            close(fd);
            return fail(storage, BT_DEBUG_SD_ERROR, error);
        }
        storage->file_bytes = 0;
        storage->state = BT_DEBUG_SD_RECORDING;
        return true;
    }
    return fail(storage, BT_DEBUG_SD_ERROR, EEXIST);
}

void bt_debug_storage_init(bt_debug_storage_t *storage, size_t file_limit, unsigned max_files)
{
    *storage = (bt_debug_storage_t){.file_limit = file_limit, .max_files = max_files};
}

bool bt_debug_storage_open(bt_debug_storage_t *storage, bool mounted, const char *mount, uint32_t session)
{
    if (!storage) return false;
    bt_debug_storage_close(storage);
    storage->bytes_written = 0;
    storage->last_errno = 0;
    storage->file_index = 0;
    storage->session = session;
    if (!mounted) return fail(storage, BT_DEBUG_SD_NO_CARD, ENODEV);
    if (!mount || !storage->file_limit || !storage->max_files) return fail(storage, BT_DEBUG_SD_ERROR, EINVAL);
    struct stat st;
    if (stat(mount, &st) != 0) return fail(storage, BT_DEBUG_SD_ERROR, errno);
    if (!S_ISDIR(st.st_mode)) return fail(storage, BT_DEBUG_SD_ERROR, ENOTDIR);
    int n = snprintf(storage->directory, sizeof(storage->directory), "%s/bt_logs", mount);
    if (n < 0 || (size_t)n >= sizeof(storage->directory)) return fail(storage, BT_DEBUG_SD_ERROR, ENAMETOOLONG);
    if (mkdir(storage->directory, 0775) != 0 && errno != EEXIST) return fail(storage, BT_DEBUG_SD_ERROR, errno);
    return create_file(storage);
}

bool bt_debug_storage_write(bt_debug_storage_t *storage, const char *data, size_t size)
{
    if (!storage || !storage->file || storage->state != BT_DEBUG_SD_RECORDING) return false;
    if (!data || size > storage->file_limit) return fail(storage, BT_DEBUG_SD_ERROR, EINVAL);
    if (storage->file_bytes > storage->file_limit - size) {
        if (fclose(storage->file) != 0) {
            storage->file = NULL;
            return fail(storage, BT_DEBUG_SD_ERROR, errno);
        }
        storage->file = NULL;
        if (++storage->file_index >= storage->max_files) return fail(storage, BT_DEBUG_SD_LIMIT, 0);
        if (!create_file(storage)) return false;
    }
    if (fwrite(data, 1, size, storage->file) != size) return fail(storage, BT_DEBUG_SD_ERROR, errno ? errno : EIO);
    storage->file_bytes += size;
    storage->bytes_written += size;
    return true;
}

bool bt_debug_storage_flush(bt_debug_storage_t *storage)
{
    if (!storage || !storage->file) return false;
    if (fflush(storage->file) != 0) return fail(storage, BT_DEBUG_SD_ERROR, errno);
    if (fsync(fileno(storage->file)) != 0) return fail(storage, BT_DEBUG_SD_ERROR, errno);
    return true;
}

void bt_debug_storage_close(bt_debug_storage_t *storage)
{
    if (!storage) return;
    if (storage->file && fclose(storage->file) != 0) {
        storage->file = NULL;
        storage->state = BT_DEBUG_SD_ERROR;
        storage->last_errno = errno;
        return;
    }
    storage->file = NULL;
    storage->state = BT_DEBUG_SD_OFF;
}
