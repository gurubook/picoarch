#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "core.h"
#include "content.h"
#include "patch.h"
#include "unzip.h"
#include "util.h"

static int alloc_readfile(const char *path, void **buf, size_t *size) {
	int ret = -1;
	FILE *file = fopen(path, "r");

	if (!file) {
		goto finish;
	}

	fseek(file, 0, SEEK_END);
	*size = ftell(file);
	rewind(file);

	if (!*size) {
		ret = 0;
		goto finish;
	}

	*buf = malloc(*size);
	if (!buf) {
		PA_ERROR("Couldn't allocate memory for file: %s\n", path);
		goto finish;
	}

	if (*size != fread(*buf, sizeof(uint8_t), *size, file)) {
		PA_ERROR("Error reading file: %s\n", path);
		goto finish;
	}
	ret = 0;

finish:
	if (ret) {
		free(*buf);
		*buf = NULL;
		*size = 0;
	}

	if (file)
		fclose(file);

	return ret;
}

static int content_load_zip(struct content *content) {
	const char *ext = NULL;
	int i = 0;
	int ret = -1;
	FILE *f = NULL;
	const char **extensions = core_extensions();

	if (extensions && has_suffix_i(content->path, ".zip")) {
		while((ext = extensions[i++])) {
			if (!strcmp(ext, "zip")) {
				return 0;
			}
		}

		f = fopen(content->path, "r");
		if (!f) {
			goto finish;
		}

		if (content->tmpfile)
			remove(content->tmpfile);

		free(content->tmpfile);

		content->tmpfile = calloc(MAX_PATH, sizeof(*content->tmpfile));
		if (!content->tmpfile) {
			PA_ERROR("Couldn't allocate memory for unzipped path\n");
			goto finish;
		}

		if (unzip_tmp(f, extensions, content->tmpfile, MAX_PATH)) {
			remove(content->tmpfile);
			free(content->tmpfile);
			content->tmpfile = NULL;
			goto finish;
		}
	}

	ret = 0;

finish:
	if (f)
		fclose(f);

	return ret;
}

static char *content_patch_pattern;

static int content_patch_filter(const struct dirent *ent) {
	const char *p;

	if (content_patch_pattern &&
	    !strncmp(ent->d_name, content_patch_pattern, strlen(content_patch_pattern))) {
		p = ent->d_name + strlen(content_patch_pattern);

		return !strncasecmp(p, ".ips", sizeof(".ips") - 1) ||
			!strncasecmp(p, ".bps", sizeof(".bps") - 1);
	}

	return 0;
}

static int content_patch_compare(const struct dirent **d1, const struct dirent **d2) {
	return strcasecmp((*d1)->d_name, (*d2)->d_name);
}

static int content_patch(const struct content *content, void *data, size_t size, void **out, size_t *out_size) {
	struct dirent **namelist;
	char pattern[MAX_PATH];
	char patch_path[MAX_PATH];
	int n = 0;
	char *path = strdup(content->path);
	char *dir;

	const void *in = data;
	size_t in_size = size;
	void *patch_data = NULL;
	size_t patch_size = 0;
	void *patched = NULL;
	size_t patched_size = 0;

	int i = 0;
	int ret = -1;

	dir = dirname(path);
	content_based_name(content, pattern, sizeof(pattern), NULL, NULL, "");

	content_patch_pattern = gnu_basename(pattern);
	n = scandir(dir, &namelist, content_patch_filter, content_patch_compare);
	content_patch_pattern = NULL;

	if (n < 0) {
		PA_ERROR("Error reading directory: %s\n", strerror(errno));
		goto finish;
	}

	if (n == 0) {
		goto finish;
	}

	for (i = 0; i < n; i++) {
		free(patch_data);
		snprintf(patch_path, sizeof(patch_path), "%s%s%s", dir, "/", namelist[i]->d_name);

		if (alloc_readfile(patch_path, &patch_data, &patch_size)) {
			goto finish;
		}

		if (patched) {
			if (in != data)
				free((void *)in);

			in = patched;
			in_size = patched_size;
			patched = NULL;
		}

		if (patch(in, in_size, patch_data, patch_size, &patched, &patched_size))
			goto finish;

		PA_INFO("Applied %s\n", patch_path);
	}

	*out = patched;
	*out_size = patched_size;

	ret = 0;
finish:

	while (n--) {
		free(namelist[n]);
	}
	free(namelist);

	if (in != data)
		free((void *)in);

	free(patch_data);
	free(path);
	return ret;
}

static int content_patch_file(struct content *content, const char *path) {
	int fd, outfd;
	int ret = -1;
	struct stat stat;
	void *addr;
	void *out = NULL;
	size_t out_size = 0;
	char *content_path = strdup(content->path);
	char *content_name;
	FILE *outfile = NULL;

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		PA_ERROR("Couldn't open content file\n");
		goto finish;
	}

	if (fstat(fd, &stat) == -1) {
		PA_ERROR("Couldn't read content file size\n");
		goto finish;
	}

	addr = mmap(NULL, stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED) {
		PA_ERROR("Couldn't read content file\n");
		goto finish;
	}

	if (content_patch(content, addr, stat.st_size, &out, &out_size)) {
		goto finish;
	}

	if (content->tmpfile)
		remove(content->tmpfile);

	free(content->tmpfile);

	content->tmpfile = calloc(MAX_PATH, sizeof(*content->tmpfile));
	if (!content->tmpfile) {
		PA_ERROR("Couldn't allocate memory for patched path\n");
		goto finish;
	}

	content_name = basename(content_path);
	snprintf(content->tmpfile, MAX_PATH, "/tmp/pa-XXXXXX%s", content_name);

	outfd = mkstemps(content->tmpfile, strlen(content_name));
	outfile = fdopen(outfd, "w");
	if (!outfile) {
		PA_ERROR("Error creating temporary file for patching: %s\n", strerror(errno));
		goto finish;
	}

	if (out_size != fwrite(out, 1, out_size, outfile)) {
		PA_ERROR("Error writing file %s: %s\n", path, strerror(errno));
		goto finish;
	}

	ret = 0;

finish:
	if (addr != MAP_FAILED)
		munmap(addr, stat.st_size);

	if (fd >= 0)
		close(fd);

	if (outfile)
		fclose(outfile);

	free(out);
	free(content_path);

	return ret;
}

struct content *content_init(const char *path) {
	struct content* content = calloc(1, sizeof(struct content));

	if (content) {
		strncpy((char *)content->path, path, sizeof(content->path) - 1);
	}
	return content;
}

void content_based_name(const struct content *content,
                        char *buf, size_t len,
                        const char *basedir, const char *subdir,
                        const char *new_extension) {
	char filename[MAX_PATH];
	char *path = strdup(content->path);
	char *dot;

	if (basedir) {
		if (!subdir)
			subdir = "";

		strncpy(filename, basename(path), sizeof(filename));
	} else {
		basedir = "";
		subdir = "";
		strncpy(filename, path, sizeof(filename));
	}

	filename[sizeof(filename) - 1] = 0;

	dot = strrchr(filename, '.');
	if (dot)
		*dot = 0;

	snprintf(buf, len, "%s%s%s%s", basedir, subdir, filename, new_extension);

	free(path);
}

int content_load_game_info(struct content *content, struct retro_game_info *info, bool needs_fullpath) {
	const char *path;
	int ret = -1;
	PA_INFO("Loading %s\n", content->path);

	if (content_load_zip(content)) {
		PA_ERROR("Error unzipping content file: %s\n", content->path);
		goto finish;
	}

	path = content->tmpfile ? content->tmpfile : content->path;

	if (needs_fullpath) {
		content_patch_file(content, path);
		path = content->tmpfile ? content->tmpfile : content->path;
		info->path = path;
	} else {
		void *data = NULL;
		size_t size = 0;
		void *patched_data = NULL;
		size_t patched_size = 0;

		free(content->data);
		content->data = NULL;

		if (alloc_readfile(path, &data, &size)) {
			PA_ERROR("Error reading content file: %s\n", path);
			goto finish;
		}

		if (!content_patch(content, data, size, &patched_data, &patched_size) && patched_data) {
			free(data);
			data = patched_data;
			size = patched_size;
		}

		info->path = path;
		info->data = content->data = data;
		info->size = content->size = size;
	}
	ret = 0;

finish:
	return ret;
}

void content_free(struct content *content) {
	if (!content)
		return;

	if (content->tmpfile) {
		remove(content->tmpfile);
	}

	free(content->tmpfile);
	free(content->data);
	free(content);
}
