#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#define TRACKER_PATH "/var/lib/osd/tracker"

static void tracker_canonicalize(
	const char *client, 
	const char *mount_name, 
	char path[PATH_MAX])
{
	snprintf(path, PATH_MAX, "%s/%s_%s", TRACKER_PATH, client, mount_name); 
}

int tracker_mount(const char *client, const char *mount_path) 
{
	char tracker_path[PATH_MAX], link_path[PATH_MAX];
	const char *p, *mount_name;
	int fd = -1, ret = 0;

	fprintf(stderr, "Client %s connecting to %s\n", client, mount_path);

	p = strrchr(client, '/');
	if (p != NULL) {
		fprintf(stderr, "Don't like clients with slash %s\n", client);
		ret = -1;
		goto done;
	}

	p = strrchr(mount_path, '/');
	mount_name = p ? p + 1 : mount_path;

	snprintf(tracker_path, PATH_MAX, "%s/%s", TRACKER_PATH, mount_name); 
	fd = open(tracker_path, O_CREAT | O_RDWR, 0600);
	if (fd < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", 
				tracker_path, strerror(errno));
		ret = -errno;
		goto done;
	}
	tracker_canonicalize(client, mount_name, link_path);
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
	const char *p, *mount_name;
	struct stat st;
	int ret;

	fprintf(stderr, "Client %s disconnecting from %s\n", client, mount_path);
	p = strrchr(client, '/');
	if (p != NULL) {
		fprintf(stderr, "Don't like clients with slash %s\n", client);
		ret = -1;
		goto done;
	}
	p = strrchr(mount_path, '/');
	mount_name = p ? p + 1 : mount_path;

	tracker_canonicalize(client, mount_name, path);
	if ((ret = unlink(path)) != 0) {
		fprintf(stderr, "Failed to unlink %s: %s\n", 
				path, strerror(errno));
		ret = -errno;
		goto done;
	}

	snprintf(path, PATH_MAX, "%s/%s", TRACKER_PATH, mount_name); 
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

