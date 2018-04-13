/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2011 xaizek.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "registers.h"

#include <stddef.h>   /* NULL size_t */
#include <stdarg.h>   /* va_... to declare printf-like function */
#include <stdio.h>    /* snprintf() */
#include <string.h>
#include <stdlib.h>   /* free */
#include <errno.h>    /* EEXIST etc. */

#include <unistd.h>   /* ftruncate */
#include <pthread.h>  /* mutex */
#include <fcntl.h>    /* O_RDWR, O_EXCL, O_CREAT, ... */
#include <sys/mman.h> /* mmap, shm_unlink */

#include "compat/reallocarray.h"
#include "modes/dialogs/msg_dialog.h" /* show_error_msgf */
#include "utils/fs.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "trash.h"

/* Name of the "unnamed" (the default) register. */
#define UNNAMED_REG_NAME '"'

/* Number of registers named as alphabet letters. */
#define NUM_LETTER_REGISTERS 26
/* Number of all available registers (excludes 26 uppercase letters). */
#define NUM_REGISTERS (2 + NUM_LETTER_REGISTERS)

/* Data of all registers. */
static reg_t registers[NUM_REGISTERS];

/* Names of registers + names of 26 uppercase register names + termination null
 * character. */
const char valid_registers[] = {
	BLACKHOLE_REG_NAME, UNNAMED_REG_NAME,
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'\0'
};
/* Length should be equal to number of names of existing registers + names of 26
 * uppercase registers (virtual ones) + termination null character. */
ARRAY_GUARD(valid_registers, NUM_REGISTERS + NUM_LETTER_REGISTERS + 1);

/* Shared memory register synchronization */
struct register_metadata {
	unsigned write_counter;
	size_t num_entries;
	size_t offset;
	size_t length_used;
	size_t length_available;
};

struct shared_registers {
	pthread_mutex_t shared_mutex;
	pthread_mutexattr_t mutex_attributes;

	/*
	 * size backed by shared memory file including metadata
	 * ranges between shared_initial and shared_mmap_bytes
	 */
	size_t size_backed;

	unsigned write_counter;
	size_t length_area_used; /* excluding metadata */

	/* BLACKHOLE, DEFAULT, a-z */
	struct register_metadata register_metadata[NUM_REGISTERS];
};

#define SHARED_ALL_METADATA_SIZE (sizeof(struct shared_registers))

/* Maximum length the shared memory name will take. */
#define SHARED_USE_NAME_MAX 4096

/*
 * The following defines two sizes
 *
 * shared_mmap_bytes  The number of bytes to map with mmap()
 * shared_initial     The number of bytes to map initially
 *                    which must be greater than SHARRED_ALL_METADATA_SIZE
 *                    (which is smaller than 2 KiB on amd64 Linux)
 *
 * During VIFM runtime these values should never be modified. They are
 * not constant here only to allow enalbing a test mode with smaller values to
 * test corner cases.
 */
#ifdef __linux__
/*
 * On Linux a mmap call can map more bytes than the size of the underlying
 * object. Thus the area mapped can be 128 MiB (very large) and the memory
 * actually used can start very small at 128 KiB and be resized by
 * an `ftruncate` on the file descriptor.
 */
static size_t shared_mmap_bytes = 1024 * 1024 * 128; /* 128 MiB ~ infinity */
static size_t shared_initial    = 1024 * 128;        /* 128 KiB ~ small    */
#else
/*
 * POSIX does not require mmap to be able to map beyond a file size. It is
 * even more unspecific in that
 *
 *	``The mmap() function may fail if:
 *  [EINVAL]
 *	    The addr argument (if MAP_FIXED was specified) or off is not a
 *	    multiple of the page size as returned by sysconf(), or is considered
 *	    invalid by the implementation.''
 *	(from The Open Group Base Specifications Issue 7 IEEE Std 1003.1,
 *	2013 Edition page for `mmap`)
 *
 * i.e. any value considered ``invalid by the implementation'' may fail.
 *
 * By means of experimentation (see
 * https://travis-ci.org/vifm/vifm/builds/366274721 and
 * https://github.com/vifm/vifm/pull/280#issuecomment-381245032), it was found
 * out that for OSX (at least on the CI), `mmap` on a larger area than the
 * size given to `ftruncate` fails with EINVAL.
 *
 * As it is safer to generally not rely on the (probably) Linux-specific
 * possibility to map beyond the shared memory object's current size, the mapped
 * and actual size are defined to be equal for all non-Linux systems.
 *
 * For a balance between register capacity and memory waste, 16 MiB was chosen.
 */
static size_t shared_mmap_bytes = 1024 * 1024 * 16; /* 16 MiB ~ enough? */
static size_t shared_initial    = 1024 * 1024 * 16;
#endif

static struct shared_registers* shmem = NULL;
/* pointer to the shared memory as an unstructured blob of bytes */
static char* shmem_raw = NULL;
static int shm_fd = -1;
static unsigned my_write_counter = 0;
static char debug_print_to_stdout = 0;

static void regs_sync_error(const char format[], ...);
static void regs_sync_shm_close();
static void regs_sync_shm_unlink(char* name);
static void regs_sync_shm_unmap();
static char regs_sync_to_shared_memory_critical();
static char regs_sync_enter_critical_section();
static void regs_sync_rewrite_critical();
static size_t regs_sync_store_register_contents_critical(size_t current_offset,
	size_t reg_id);
static size_t regs_sync_store_register_contents_in_place(size_t current_offset,
	size_t reg_id);
static char regs_sync_resize_allocation(size_t newsz);
static void regs_sync_leave_critical_section();

void
regs_init(void)
{
	int i;
	for(i = 0; i < NUM_REGISTERS; i++)
	{
		registers[i].name = valid_registers[i];
		registers[i].nfiles = 0;
		registers[i].files = NULL;
	}
}

int
regs_exists(int reg_name)
{
	int i;
	for(i = 0; i < NUM_REGISTERS; ++i)
	{
		if(valid_registers[i] == reg_name)
		{
			return 1;
		}
	}
	return 0;
}

reg_t *
regs_find(int reg_name)
{
	int i;
	for(i = 0; i < NUM_REGISTERS; ++i)
	{
		if(registers[i].name == reg_name)
		{
			return &registers[i];
		}
	}
	return NULL;
}

static int
check_for_duplicate_file_names(reg_t *reg, const char file[])
{
	int i;
	for(i = 0; i < reg->nfiles; ++i)
	{
		if(stroscmp(file, reg->files[i]) == 0)
		{
			return 1;
		}
	}
	return 0;
}

int
regs_append(int reg_name, const char file[])
{
	reg_t *reg;

	if(reg_name == BLACKHOLE_REG_NAME)
	{
		return 0;
	}
	if((reg = regs_find(reg_name)) == NULL)
	{
		return 1;
	}
	if(check_for_duplicate_file_names(reg, file))
	{
		return 1;
	}

	reg->nfiles = add_to_string_array(&reg->files, reg->nfiles, 1, file);
	return 0;
}

void
regs_reset(void)
{
	const char *p = valid_registers;
	while(*p != '\0')
	{
		regs_clear(*p++);
	}
}

void
regs_clear(int reg_name)
{
	reg_t *const reg = regs_find(reg_name);
	if(reg == NULL)
	{
		return;
	}

	free_string_array(reg->files, reg->nfiles);
	reg->files = NULL;
	reg->nfiles = 0;
}

void
regs_pack(int reg_name)
{
	int j, i;
	reg_t *const reg = regs_find(reg_name);
	if(reg == NULL)
	{
		return;
	}

	j = 0;
	for(i = 0; i < reg->nfiles; ++i)
	{
		if(reg->files[i] != NULL)
		{
			reg->files[j++] = reg->files[i];
		}
	}
	reg->nfiles = j;
}

char **
regs_list(const char registers[])
{
	char **list = NULL;
	size_t len = 0;

	while(*registers != '\0')
	{
		reg_t *reg = regs_find(*registers++);
		char reg_str[16];
		int i;

		if(reg == NULL || reg->nfiles <= 0)
		{
			continue;
		}

		snprintf(reg_str, sizeof(reg_str), "\"%c", reg->name);
		len = add_to_string_array(&list, len, 1, reg_str);

		i = reg->nfiles;
		while(i-- > 0)
		{
			len = add_to_string_array(&list, len, 1, reg->files[i]);
		}
	}

	(void)put_into_string_array(&list, len, NULL);
	return list;
}

void
regs_rename_contents(const char old[], const char new[])
{
	int i;
	for(i = 0; i < NUM_REGISTERS; ++i)
	{
		int j;
		const int n = registers[i].nfiles;
		for(j = 0; j < n; ++j)
		{
			if(stroscmp(registers[i].files[j], old) != 0)
				continue;

			(void)replace_string(&registers[i].files[j], new);
			/* Registers don't contain duplicates, so exit this loop. */
			break;
		}
	}
}

void
regs_remove_trashed_files(const char trash_dir[])
{
	int i;
	for(i = 0; i < NUM_REGISTERS; ++i)
	{
		int j, needs_packing = 0;
		const int n = registers[i].nfiles;
		for(j = 0; j < n; ++j)
		{
			if(!trash_contains(trash_dir, registers[i].files[j]))
				continue;
			if(!path_exists(registers[i].files[j], DEREF))
				continue;

			update_string(&registers[i].files[j], NULL);
			needs_packing = 1;
		}
		if(needs_packing)
		{
			regs_pack(registers[i].name);
		}
	}
}

void
regs_update_unnamed(int reg_name)
{
	reg_t *unnamed, *reg;
	int i;

	if(reg_name == UNNAMED_REG_NAME)
		return;

	if((reg = regs_find(reg_name)) == NULL)
		return;

	if((unnamed = regs_find(UNNAMED_REG_NAME)) == NULL)
		return;

	regs_clear(UNNAMED_REG_NAME);

	unnamed->nfiles = reg->nfiles;
	unnamed->files = reallocarray(unnamed->files, unnamed->nfiles,
			sizeof(char *));
	for(i = 0; i < unnamed->nfiles; ++i)
	{
		unnamed->files[i] = strdup(reg->files[i]);
	}
}

void
regs_suggest(regs_suggest_cb cb, int max_entries_per_reg)
{
	wchar_t reg_name[] = L"reg: X";
	const char *registers = valid_registers;

	while(*registers != '\0')
	{
		reg_t *const reg = regs_find(*registers++);
		int i, max = max_entries_per_reg;

		if(reg == NULL || reg->nfiles <= 0)
		{
			continue;
		}

		i = reg->nfiles - 1;

		reg_name[5] = reg->name;
		cb(reg_name, L"", replace_home_part(reg->files[i]));

		while(--max > 0 && i-- > 0)
		{
			cb(L"", L"", replace_home_part(reg->files[i]));
		}
	}
}

/* Shared memory synchronization of register contents -- implementation */

void
regs_sync_enable(char* shared_memory_name)
{
	char error_other;
	char error_excl_already_exists;
	char error_normal_does_not_exist;

	/*
	 * In order to allow shared memory names to be written without knowing the
	 * details of the shared memory implementation (e.g. Linux needs a leading
	 * slash for the shared memory name), a different name than the one supplied
	 * is actually used. Currently, the provided name is just prefixed by
	 * /vifm-
	 */
	char use_name[SHARED_USE_NAME_MAX];
	strcpy(use_name, "/vifm-");
	strncpy(use_name + 6, shared_memory_name, SHARED_USE_NAME_MAX - 6);

	/*
	 * Disable feature if it was active before.
	 * It is not sufficient to return if the feature is already enabled because
	 * the shared_memory_name may have changed.
	 */
	regs_sync_disable();

	do {
		/* reset errors */
		error_other = 0;
		error_excl_already_exists = 0;
		error_normal_does_not_exist = 0;

		/* try exclusivie */
		shm_fd = shm_open(use_name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if(shm_fd == -1) {
			/* error exclusive */
			if((error_excl_already_exists = (errno == EEXIST))) {
				/* already exists => try to attach to existing */
				shm_fd = shm_open(use_name, O_RDWR, 0600);
				if(shm_fd == -1) {
					/* we first got EEXIST now ENOENT => file deleted in the meantime
					 * => retry! */
					error_normal_does_not_exist = (errno == ENOENT);
					error_other = !error_normal_does_not_exist;
				} /* else: successfully attached to existing */
			} else {
				error_other = 1;
			}
		} /* else: no error exclusive / we know we are init! */
	} while(!error_other && error_excl_already_exists &&
		error_normal_does_not_exist);

	/*
	 * Possible cases:
	 * (1) error_other                => FAIL
	 * (2) error_excl_already_exists  => OK, just attached normally
	 * (3) !error_excl_already_exists => OK, responsible for initialization
	 */

	if(error_other) {
		regs_sync_error("Failed to open/create shared memory object: %s",
			strerror(errno));
		return;
	}

	/* initialization routine part 1 */
	if(!error_excl_already_exists && ftruncate(shm_fd, shared_initial) == -1) {
		regs_sync_error("Failed to resize shared memory "
			"(during initialization): %s", strerror(errno));
		regs_sync_shm_close();
		regs_sync_shm_unlink(shared_memory_name);
		return;
	}

	/* map shared memory */
	shmem_raw = mmap(NULL, shared_mmap_bytes,
		PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

	if(shmem_raw == MAP_FAILED) {
		regs_sync_error("Failed to mmap into shared memory area: %s",
			strerror(errno));
		regs_sync_shm_close();
		if(!error_excl_already_exists)
			regs_sync_shm_unlink(shared_memory_name);
		return;
	}

	/* structured view on the same data */
	shmem = (struct shared_registers*)shmem_raw;

	/* initialization routine part 2 */
	if(!error_excl_already_exists) {
		/*
		 * Race condition: What if we are initializing and another instance is just
		 * starting where the mutex does not exist yet? Then the assumption is that
		 * while it has enabled the shared memory feature and created the mapping,
		 * it will not immediately access it thereafter such that the running
		 * initialization has enough time to finish.
		 */
		if((pthread_mutexattr_init(&shmem->mutex_attributes) != 0) ||
				(pthread_mutexattr_setpshared(&shmem->mutex_attributes,
					PTHREAD_PROCESS_SHARED) != 0) ||
				(pthread_mutex_init(&shmem->shared_mutex,
					&shmem->mutex_attributes) != 0)) {
			regs_sync_error("Failed to initialize mutex: %s", strerror(errno));
			regs_sync_shm_unmap();
			regs_sync_shm_close();
			regs_sync_shm_unlink(shared_memory_name);
			return;
		}

		if(!regs_sync_enter_critical_section()) {
			regs_sync_shm_unmap();
			regs_sync_shm_close();
			regs_sync_shm_unlink(shared_memory_name);
			return;
		}

		my_write_counter = shmem->write_counter;
		shmem->size_backed = shared_initial;

		if(regs_sync_to_shared_memory_critical())
			regs_sync_leave_critical_section();
	}
}

static void
regs_sync_error(const char format[], ...)
{
	va_list ap;
	va_start(ap, format);
	if(debug_print_to_stdout) {
		printf("error,");
		vprintf(format, ap);
		putchar('\n');
	} else{
		show_error_msgf("Error in Shared Memory Register "
			"Synchronization", format, ap);
	}
	va_end(ap);
}

static void
regs_sync_shm_close()
{
	if(close(shm_fd))
		regs_sync_error("Failed to close shared memory: %s", strerror(errno));
}

static void
regs_sync_shm_unlink(char* name)
{
	if(shm_unlink(name))
		regs_sync_error("Failed to close shared memory: %s", strerror(errno));
}

static void
regs_sync_shm_unmap()
{
	if(munmap(shmem_raw, shared_mmap_bytes))
		regs_sync_error("Failed to unmap shared memory: %s", strerror(errno));
}

void
regs_sync_disable()
{
	if(shmem != NULL) {
		regs_sync_shm_unmap();
		regs_sync_shm_close();
		shmem = NULL;
	}
}

void
regs_sync_to_shared_memory()
{
	if(regs_sync_enter_critical_section() &&
			regs_sync_to_shared_memory_critical())
		regs_sync_leave_critical_section();
}

static char
regs_sync_to_shared_memory_critical()
{
	/* returns 1 on success, 0 on failure (cleans up as needed on fail) */

	my_write_counter = ++shmem->write_counter;

	/* determine memory requirements for state to be synchronized */
	size_t new_register_sizes_total = 0;
	size_t new_register_sizes[NUM_REGISTERS];

	size_t i;
	size_t j;

	for(i = 0; i < NUM_REGISTERS; ++i) {
		new_register_sizes[i] = 0;
		for(j = 0; j < registers[i].nfiles; j++) {
			/* +1 to count the 0 terminator byte */
			new_register_sizes[i] += strlen(registers[i].files[j]) + 1;
		}
		new_register_sizes_total += new_register_sizes[i];
	}

	/* check which registers grow over their currently available space */
	size_t size_not_fit_to_existing = 0;
	for(i = 0; i < NUM_REGISTERS; ++i) {
		if(new_register_sizes[i] > shmem->register_metadata[i].length_available)
			size_not_fit_to_existing += new_register_sizes[i];
	}

	if(size_not_fit_to_existing <= shmem->size_backed - SHARED_ALL_METADATA_SIZE -
			shmem->length_area_used) {
		size_t halved_sz = shmem->size_backed / 2;
		if(new_register_sizes_total < (halved_sz - SHARED_ALL_METADATA_SIZE)
				&& shmem->size_backed > shared_initial) {
			/* halve allocation size */
			if(!regs_sync_resize_allocation(halved_sz))
				return 0;
			regs_sync_rewrite_critical();
		} else {
			size_t offset = SHARED_ALL_METADATA_SIZE + shmem->length_area_used;
			for(i = 0; i < NUM_REGISTERS; ++i) {
				if(new_register_sizes[i] >
						shmem->register_metadata[i].length_available) {
					/* append to end */
					offset = regs_sync_store_register_contents_critical(offset, i);
				} else {
					/* update in place */
					regs_sync_store_register_contents_in_place(
						shmem->register_metadata[i].offset, i);
				}
			}
			shmem->length_area_used = offset - SHARED_ALL_METADATA_SIZE;
		}
	} else {
		/* double allocation size till fit */
		size_t new_size_backed = shmem->size_backed;
		while(new_register_sizes_total >
				(new_size_backed - SHARED_ALL_METADATA_SIZE))
			new_size_backed *= 2;

		if(new_size_backed != shmem->size_backed &&
				!regs_sync_resize_allocation(new_size_backed))
			return 0;

		regs_sync_rewrite_critical();
	}

	return 1;
}

static char
regs_sync_enter_critical_section()
{
	/* return 0 to cancel further processing */
	if(shmem == NULL)
		return 0;

	if(pthread_mutex_lock(&shmem->shared_mutex)) {
		regs_sync_error("Failed to lock mutex: %s", strerror(errno));
		return 0;
	}

	return 1;
}

static void
regs_sync_rewrite_critical()
{
	/* assumption: enough space in shared memory */
	size_t offset = SHARED_ALL_METADATA_SIZE;
	size_t i;
	for(i = 0; i < NUM_REGISTERS; ++i)
		offset = regs_sync_store_register_contents_critical(offset, i);
	shmem->length_area_used = offset - SHARED_ALL_METADATA_SIZE;
}

static size_t
regs_sync_store_register_contents_critical(size_t current_offset, size_t reg_id)
{
	/* returns new offset */
	size_t new_offset =
		regs_sync_store_register_contents_in_place(current_offset, reg_id);
	shmem->register_metadata[reg_id].length_available =
		shmem->register_metadata[reg_id].length_used;
	return new_offset;
}

static size_t
regs_sync_store_register_contents_in_place(size_t current_offset, size_t reg_id)
{
	size_t i;
	shmem->register_metadata[reg_id].write_counter = my_write_counter;
	shmem->register_metadata[reg_id].num_entries   = registers[reg_id].nfiles;
	shmem->register_metadata[reg_id].offset        = current_offset;
	for(i = 0; i < registers[reg_id].nfiles; ++i) {
		size_t entry_len = strlen(registers[reg_id].files[i]) + 1;
		memcpy(shmem_raw + current_offset, registers[reg_id].files[i], entry_len);
		current_offset += entry_len;
	}
	shmem->register_metadata[reg_id].length_used =
		current_offset - shmem->register_metadata[reg_id].offset;
	return current_offset;
}

static char
regs_sync_resize_allocation(size_t newsz)
{
	/* returns 1 on success, 0 on failure (performs cleanup for failure case) */
	if(newsz > shared_mmap_bytes || ftruncate(shm_fd, newsz)) {
		/* shared memory ends here */
		regs_sync_error("Shared memory size exceeded: %s", strerror(errno));
		regs_sync_disable();
		return 0;
	}
	/* else: truncate successful */
	shmem->size_backed = newsz;
	return 1;
}

static void
regs_sync_leave_critical_section()
{
	if(pthread_mutex_unlock(&shmem->shared_mutex))
		regs_sync_error("Failed to unlock mutex: %s", strerror(errno));
}

void
regs_sync_from_shared_memory()
{
	if(!regs_sync_enter_critical_section())
		return;

	size_t i;
	size_t j;

	if(shmem->write_counter != my_write_counter) {
		/* Other instance canged the register contents, let's check
		 * the details. */
		for(i = 0; i < NUM_REGISTERS; ++i) {
			if(shmem->register_metadata[i].write_counter > my_write_counter) {
				for(j = 0; j < registers[i].nfiles; ++j)
					free(registers[i].files[j]);
				free(registers[i].files);
				registers[i].nfiles = shmem->register_metadata[i].num_entries;
				registers[i].files = malloc(registers[i].nfiles * sizeof(char*));
				char* curstrptr = shmem_raw + shmem->register_metadata[i].offset;
				for(j = 0; j < registers[i].nfiles; ++j) {
					size_t curlen = strlen(curstrptr) + 1;
					registers[i].files[j] = malloc(curlen * sizeof(char));
					memcpy(registers[i].files[j], curstrptr, curlen);
					curstrptr += curlen;
				}
			}
		}
		my_write_counter = shmem->write_counter;
	}

	regs_sync_leave_critical_section();
}

static void regs_sync_debug_print_area(size_t offset, size_t length)
{
	size_t i;
	for(i = 0; i < length; ++i) {
		char val = shmem_raw[offset + i];
		if(val == 0)
			printf("(0)");
		else
			putchar(val);
	}
	putchar('\n');
}

void regs_sync_debug_print_memory()
{
	size_t i;
	size_t j;

	printf("-- BEGIN VIFM shared memory synchronization DUMP --\n");
	printf("| local\n");
	for(i = 0; i < NUM_REGISTERS; ++i) {
		printf("| | register %2lu name=%c, nfiles=%d, files=\n", i,
			registers[i].name, registers[i].nfiles);
		for(j = 0; j < registers[i].nfiles; ++j)
			printf("| | | %s\n", registers[i].files[j]);
	}
	printf("| meta shmem=%lx, shmem_raw=%lx, shm_fd=%d, my_write_counter=%u\n",
		(unsigned long)shmem, (unsigned long)shmem_raw, shm_fd, my_write_counter);
	if(shmem != NULL) {
		printf("| shmem size_backed=%lu, write_counter=%u, length_area_used=%lu, "
			"register_metadata=\n", shmem->size_backed, shmem->write_counter,
			shmem->length_area_used);
		for(i = 0; i < NUM_REGISTERS; ++i) {
			printf(
				"| | register %2lu write_counter=%u, num_entries=%lu, "
					"offset=%lu, length_used=%lu, length_available=%lu\n",
				i,
				shmem->register_metadata[i].write_counter,
				shmem->register_metadata[i].num_entries,
				shmem->register_metadata[i].offset,
				shmem->register_metadata[i].length_used,
				shmem->register_metadata[i].length_available
			);
			printf("| | | used  =");
			regs_sync_debug_print_area(shmem->register_metadata[i].offset,
				shmem->register_metadata[i].length_used);
			printf("| | | unused=");
			regs_sync_debug_print_area(
				shmem->register_metadata[i].length_used,
				shmem->register_metadata[i].length_available - 
					shmem->register_metadata[i].length_used
			);
		}
	}
	printf("-- END   VIFM shared memory synchronization DUMP --\n");
}

void regs_sync_enable_test_mode()
{
	debug_print_to_stdout = 1;
#ifdef __linux__
	shared_mmap_bytes     = 1024 * 32;
	shared_initial        = 1024 * 4;  /* still larger than metadata size */
#else
	/* Note that many of the tests test resizing and are thus implicitly
	 * useless on non-Linux platforms. See the variable definitions for why
	 * other Unix-like OS are treated differently from Linux here. */
	shared_mmap_bytes     = 1024 * 32;
	shared_initial        = 1024 * 32;
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
