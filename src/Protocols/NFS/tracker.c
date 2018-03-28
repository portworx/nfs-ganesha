#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#define TRACKER_PATH "/var/lib/osd/tracker"
#define UNKNOWN_CLIENT "uc"

static void __tracker_canonicalize(
	const char *client, 
	const char *mount_name, 
	char path[PATH_MAX])
{
	snprintf(path, PATH_MAX, "%s/%s_%s", TRACKER_PATH, client, mount_name); 
}

static const char * __tracker_dirname(const char *path)
{
	const char *p = strrchr(path, '/');
	return p ? p + 1 : path;
}

static void __tracker_path(const char *mount_path, char out_path[PATH_MAX])
{
	const char *mount_name = __tracker_dirname(mount_path);
	snprintf(out_path, PATH_MAX, "%s/%s", TRACKER_PATH, mount_name); 
}

static int __tracker_find_client(const char *mount_path, char client[PATH_MAX])
{
	DIR *dirp = NULL;
	int ret = ENOENT;
	struct dirent *dp;
	const char *mount_name;

	dirp = opendir(TRACKER_PATH);
	if (dirp == NULL) {
		fprintf(stderr, "Failed to open %s: %s\n", 
				TRACKER_PATH, strerror(errno));
		ret = -errno;
		goto done;
	}

	mount_name = __tracker_dirname(mount_path);
	while ((dp = readdir(dirp)) != NULL) {
		if (strstr(dp->d_name, mount_name) != NULL &&
			strstr(dp->d_name, UNKNOWN_CLIENT) != NULL &&
			strcmp(dp->d_name, mount_name)) {
			snprintf(client, PATH_MAX, "%s/%s", 
				TRACKER_PATH, dp->d_name);
			ret = 0;
			goto done;
		}
	}

done:
	if (dirp != NULL) {
		closedir(dirp);
	}
	return ret;
}

static int __tracker_client_path(
	const char *client,
	const char *mount_path,
	char out_path[PATH_MAX])
{
	int ret = ENOENT;
	const char *mount_name;
	struct stat st;

	assert(client);;

	if (strrchr(client, '/') != NULL) {
		fprintf(stderr, "Don't like clients with slash %s\n", client);
		ret = -1;
		goto done;
	}
	mount_name = __tracker_dirname(mount_path);
	__tracker_canonicalize(client, mount_name, out_path);
	ret = stat(out_path, &st);

done:
	return ret;
}

static int __tracker_rand_client_path(
	const char *mount_path,
	char out_path[PATH_MAX])
{
	char client[256];
	int exists, retries;

	for (exists = 1, retries = 10; exists && retries > 0;  --retries) {
		snprintf(client, sizeof(client), "%s_%d", UNKNOWN_CLIENT, rand());
		exists = __tracker_client_path(client, mount_path, out_path) == 0;
	}

	return exists;
}

int tracker_mount(const char *client, const char *mount_path) 
{
	char tracker_path[PATH_MAX], link_path[PATH_MAX];
	int fd = -1, ret = 0;

	ret = client == NULL || strcmp(client, "") == 0 
		? __tracker_rand_client_path(mount_path, link_path)
		: !__tracker_client_path(client, mount_path, link_path);
	if (ret != 0) {
		fprintf(stderr, "Failed to create client mount path (%s, %s)\n",
				(client ? : "NULL"), (mount_path ? : "NULL"));
		goto done;
	}

	__tracker_path(mount_path, tracker_path);
	if ((fd = open(tracker_path, O_CREAT | O_RDWR, 0600)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", 
				tracker_path, strerror(errno));
		ret = -errno;
		goto done;
	}

	if ((ret = link(tracker_path, link_path)) != 0) {
		fprintf(stderr, "Failed to link %s->%s: %s\n", 
				tracker_path, link_path, strerror(errno));
		ret = -errno;
		goto done;
	}

done:
	if (fd >= 0) 
		close(fd);
	return ret;
}

int tracker_unmount(const char *client, const char *mount_path)
{
	char path[PATH_MAX];
	struct stat st;
	int ret;

	ret = client == NULL || strcmp(client, "") == 0 
		? __tracker_find_client(mount_path, path)
		: __tracker_client_path(client, mount_path, path);

	if (ret != 0) {
		fprintf(stderr, "Failed to find client mount path (%s, %s)\n",
				(client ? : "NULL"), (mount_path ? : "NULL"));
		goto done;
	}

	if ((ret = unlink(path)) != 0) {
		fprintf(stderr, "Failed to unlink %s: %s\n", 
				path, strerror(errno));
		ret = -errno;
		goto done;
	}

	__tracker_path(mount_path, path);
	if ((ret = stat(path, &st)) != 0) {
		fprintf(stderr, "Failed to stat %s: %s\n", 
				path, strerror(errno));
		ret = -errno;
		goto done;
	}

	if (st.st_nlink == 1) {
		if ((ret = unlink(path)) != 0) {
			fprintf(stderr, "Failed to unlink %s: %s\n", 
					path, strerror(errno));
			ret = -errno;
			goto done;
		}
	}

done:
	return ret;
}

int tracker_init()
{
	if (mkdir(TRACKER_PATH, 0777) && (errno != EEXIST)) {
		perror("Could not create NFS export directory.");
		return -errno;
	}
	return 0;
}

#ifdef UNIT_TEST
int main() 
{
	char tracker_path[PATH_MAX], link_path[PATH_MAX];
	struct stat st;

	assert(tracker_init() == 0);;
	__tracker_path("/foo", tracker_path);

	tracker_mount(NULL, "/foo");
	assert(stat(tracker_path, &st) == 0);

	tracker_mount(NULL, "/foo");
	assert(stat(tracker_path, &st) == 0);

	tracker_mount("c1", "/foo");
	assert(stat(tracker_path, &st) == 0);

	tracker_unmount(NULL, "/foo");
	assert(stat(tracker_path, &st) == 0);

	tracker_mount("c2", "/foo");
	assert(stat(tracker_path, &st) == 0);

	tracker_unmount(NULL, "/foo");
	assert(stat(tracker_path, &st) == 0);

	tracker_unmount("c1", "/foo");
	assert(stat(tracker_path, &st) == 0);

	tracker_unmount("c2", "/foo");
	assert(stat(tracker_path, &st) != 0);
}
#endif

