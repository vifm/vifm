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
#include <stdio.h>    /* snprintf() */
#include <string.h>   /* memmove() */
#include <stdlib.h>   /* free */

#include <fcntl.h>    /* O_RDWR, O_EXCL, O_CREAT, ... */
#include <unistd.h>   /* ftruncate */

#include "compat/reallocarray.h"
#include "modes/dialogs/msg_dialog.h" /* show_error_msgf */
#include "utils/fs.h"
#include "utils/gmux.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/shmem.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
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

/* Describes register contents in a shared memory. */
typedef struct
{
	unsigned int generation; /* Generation of the data. */
	size_t num_entries;      /* Number of file paths in the register. */
	size_t offset;           /* Offset of the first path. */
	size_t length_used;      /* Length currently used. */
	size_t length_available; /* Total length available for the register. */
}
reg_metadata_t;

/* Describes shared state. */
typedef struct
{
	/* Whether data is in consistent state.  Probably not bulletproof, but still
	 * an attempt to avoid using broken data. */
	int data_is_consistent;

	/* Size backed by shared memory file including metadata ranges between
	 * shared_initial and shared_mmap_bytes. */
	size_t size_backed;

	unsigned int generation; /* Generation of the data. */
	size_t length_area_used; /* Length without metadata. */

	reg_metadata_t reg_metadata[NUM_REGISTERS]; /* BLACKHOLE, DEFAULT, a-z */
}
shared_state_t;

/* Size of the metadata. */
#define SHARED_ALL_METADATA_SIZE (sizeof(shared_state_t))

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

/* Mutex that protects shared memory of shmem_obj. */
static gmux_t *shmem_gmux;
/* Shared memory area object. */
static shmem_t *shmem_obj;
/* Pointer to the shared memory as an unstructured blob of bytes. */
static char *shmem_raw;
/* Pointer to shared memory as a structure. */
static shared_state_t *shmem;
/* Last generation number that we've seen. */
static unsigned int seen_generation;
/* Whether we're in debug mode. */
static int debug_print_to_stdout;

static int find_in_reg(const reg_t *reg, const char file[]);
static void regs_sync_error(const char msg[]);
static int regs_sync_to_shared_memory_critical(void);
static int regs_sync_enter_critical_section(void);
static void regs_sync_rewrite_critical(void);
static size_t regs_sync_store_register_contents_critical(size_t current_offset,
	size_t reg_id);
static size_t regs_sync_store_register_contents_in_place(size_t current_offset,
	size_t reg_id);
static int regs_sync_resize_allocation(size_t newsz);
static void regs_sync_leave_critical_section(void);
TSTATIC int regs_sync_enabled(void);
TSTATIC void regs_sync_debug_print_memory(void);
static void regs_sync_debug_print_area(size_t offset, size_t length);
TSTATIC void regs_sync_enable_test_mode(void);

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
	return char_is_one_of(valid_registers, reg_name);
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

int
regs_append(int reg_name, const char file[])
{
	if(reg_name == BLACKHOLE_REG_NAME)
	{
		return 0;
	}

	reg_t *reg = regs_find(reg_name);
	if(reg == NULL)
	{
		return 1;
	}

	int pos = find_in_reg(reg, file);
	if(pos >= 0)
	{
		return 1;
	}
	pos = -(pos + 1);

	const int nfiles = add_to_string_array(&reg->files, reg->nfiles, file);
	if(nfiles == reg->nfiles)
	{
		return 1;
	}
	reg->nfiles = nfiles;

	char *file_copy = reg->files[nfiles - 1];
	memmove(reg->files + pos + 1, reg->files + pos,
			sizeof(*reg->files)*(nfiles - 1 - pos));
	reg->files[pos] = file_copy;
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
		len = add_to_string_array(&list, len, reg_str);

		i = reg->nfiles;
		while(i-- > 0)
		{
			len = add_to_string_array(&list, len, reg->files[i]);
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
		/* Registers don't contain duplicates, so updating single element is
		 * enough. */
		const int pos = find_in_reg(&registers[i], old);
		if(pos >= 0)
		{
			(void)replace_string(&registers[i].files[pos], new);
		}
	}
}

/* Finds position of a file in a register or whereto it should be inserted in
 * its files array.  Returns non-negative number of successful search and
 * negative index offset by one otherwise (0 -> -1, 1 -> -2, etc.). */
static int
find_in_reg(const reg_t *reg, const char file[])
{
	int l = 0;
	int u = reg->nfiles - 1;
	while(l <= u)
	{
		const int i = l + (u - l)/2;
		const int cmp = stroscmp(reg->files[i], file);
		if(cmp == 0)
		{
			return i;
		}
		else if(cmp < 0)
		{
			l = i + 1;
		}
		else
		{
			u = i - 1;
		}
	}
	return -l - 1;
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
			if(!trash_has_path_at(trash_dir, registers[i].files[j]))
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
regs_sync_enable(const char shared_memory_name[])
{
	char use_name[NAME_MAX + 1];
	snprintf(use_name, sizeof(use_name), "regs-%s", shared_memory_name);

	/* Disable feature if it was active before.
	 * It is not sufficient to return if the feature is already enabled because
	 * the shared_memory_name may have changed. */
	regs_sync_disable();

	shmem_gmux = gmux_create(use_name);
	if(shmem_gmux == NULL)
	{
		regs_sync_error("Failed to open/create shared mutex object.");
		return;
	}

	if(gmux_lock(shmem_gmux) != 0)
	{
		regs_sync_disable();
		regs_sync_error("Failed to lock shared mutex object.");
		return;
	}

	shmem_obj = shmem_create(use_name, shared_initial, shared_mmap_bytes);
	if(shmem_obj == NULL)
	{
		/* Disabling will unlock the mutex. */
		regs_sync_disable();
		regs_sync_error("Failed to open/create shared memory object.");
		return;
	}

	shmem_raw = shmem_get_ptr(shmem_obj);

	/* structured view on the same data */
	shmem = (shared_state_t *)shmem_raw;

	/* Initialization of just created shared memory area. */
	if(shmem_created_by_us(shmem_obj))
	{
		seen_generation = shmem->generation;
		shmem->data_is_consistent = 0;
		shmem->size_backed = shared_initial;

		if(!regs_sync_to_shared_memory_critical())
		{
			shmem_destroy(shmem_obj);
			shmem_obj = NULL;
			regs_sync_disable();
			regs_sync_error("Failed to initialize shared memory with data.");
			return;
		}
	}

	regs_sync_leave_critical_section();
}

/* Displays the message either in a dialog or on stdout. */
static void
regs_sync_error(const char msg[])
{
	if(debug_print_to_stdout)
	{
		printf("error,%s\n", msg);
	}
	else
	{
		show_error_msg("Error in Shared Memory Register Synchronization", msg);
	}
}

void
regs_sync_disable(void)
{
	gmux_free(shmem_gmux);
	shmem_gmux = NULL;

	shmem_free(shmem_obj);
	shmem_obj = NULL;

	shmem = NULL;
}

void
regs_sync_to_shared_memory(void)
{
	if(regs_sync_enter_critical_section())
	{
		regs_sync_to_shared_memory_critical();
		regs_sync_leave_critical_section();
	}
}

/* Puts contents of our registers into shared memory.  Returns 1 on success, 0
 * on failure (cleans up as needed on fail). */
static int
regs_sync_to_shared_memory_critical(void)
{
	shmem->data_is_consistent = 0;
	seen_generation = ++shmem->generation;

	/* Determine memory requirements for state to be synchronized. */
	size_t new_register_sizes_total = 0;
	size_t new_register_sizes[NUM_REGISTERS];

	int i;
	int j;

	for(i = 0; i < NUM_REGISTERS; ++i)
	{
		new_register_sizes[i] = 0;
		for(j = 0; j < registers[i].nfiles; ++j)
		{
			/* +1 to count the 0 terminator byte. */
			new_register_sizes[i] += strlen(registers[i].files[j]) + 1;
		}
		new_register_sizes_total += new_register_sizes[i];
	}

	/* Check which registers grow over their currently available space. */
	size_t size_not_fit_to_existing = 0;
	for(i = 0; i < NUM_REGISTERS; ++i)
	{
		if(new_register_sizes[i] > shmem->reg_metadata[i].length_available)
		{
			size_not_fit_to_existing += new_register_sizes[i];
		}
	}

	if(size_not_fit_to_existing <= shmem->size_backed - SHARED_ALL_METADATA_SIZE -
			shmem->length_area_used)
	{
		const size_t halved_size = shmem->size_backed/2;
		if(new_register_sizes_total < (halved_size - SHARED_ALL_METADATA_SIZE)
				&& shmem->size_backed > shared_initial)
		{
			/* Halve allocation size. */
			if(!regs_sync_resize_allocation(halved_size))
			{
				return 0;
			}
			regs_sync_rewrite_critical();
		}
		else
		{
			size_t offset = SHARED_ALL_METADATA_SIZE + shmem->length_area_used;
			for(i = 0; i < NUM_REGISTERS; ++i)
			{
				if(new_register_sizes[i] >
						shmem->reg_metadata[i].length_available)
				{
					/* Append at the end. */
					offset = regs_sync_store_register_contents_critical(offset, i);
				}
				else
				{
					/* Update in place. */
					regs_sync_store_register_contents_in_place(
						shmem->reg_metadata[i].offset, i);
				}
			}
			shmem->length_area_used = offset - SHARED_ALL_METADATA_SIZE;
		}
	} else {
		/* Double allocation size till fit. */
		size_t new_size_backed = shmem->size_backed;
		while(new_register_sizes_total >
				(new_size_backed - SHARED_ALL_METADATA_SIZE))
		{
			new_size_backed *= 2;
		}

		if(new_size_backed != shmem->size_backed &&
				!regs_sync_resize_allocation(new_size_backed))
		{
			return 0;
		}

		regs_sync_rewrite_critical();
	}

	return 1;
}

/* Locks the mutex protecting shared memory.  Returns non-zero on success,
 * otherwise zero is returned. */
static int
regs_sync_enter_critical_section(void)
{
	if(!regs_sync_enabled())
	{
		/* Return 0 to cancel further processing. */
		return 0;
	}

	if(gmux_lock(shmem_gmux) != 0)
	{
		regs_sync_error("Failed to lock mutex.");
		return 0;
	}

	return 1;
}

/* Rewrites shared memory from scratch. */
static void
regs_sync_rewrite_critical(void)
{
	/* Assumption: enough space in shared memory. */
	size_t offset = SHARED_ALL_METADATA_SIZE;
	int i;
	for(i = 0; i < NUM_REGISTERS; ++i)
	{
		offset = regs_sync_store_register_contents_critical(offset, i);
	}
	shmem->length_area_used = offset - SHARED_ALL_METADATA_SIZE;
}

/* Dumps contents of a register into shared memory at specified offset anew.
 * Returns new offset. */
static size_t
regs_sync_store_register_contents_critical(size_t current_offset, size_t reg_id)
{
	const size_t new_offset =
		regs_sync_store_register_contents_in_place(current_offset, reg_id);
	shmem->reg_metadata[reg_id].length_available =
		shmem->reg_metadata[reg_id].length_used;
	return new_offset;
}

/* Dumps contents of a register into shared memory at specified offset.
 * Returns new offset. */
static size_t
regs_sync_store_register_contents_in_place(size_t current_offset, size_t reg_id)
{
	int i;
	shmem->reg_metadata[reg_id].generation  = seen_generation;
	shmem->reg_metadata[reg_id].num_entries = registers[reg_id].nfiles;
	shmem->reg_metadata[reg_id].offset      = current_offset;
	for(i = 0; i < registers[reg_id].nfiles; ++i)
	{
		const size_t entry_len = strlen(registers[reg_id].files[i]) + 1;
		memcpy(shmem_raw + current_offset, registers[reg_id].files[i], entry_len);
		current_offset += entry_len;
	}
	shmem->reg_metadata[reg_id].length_used =
		current_offset - shmem->reg_metadata[reg_id].offset;
	return current_offset;
}

/* Changes size of shared area.  Returns 1 on success, 0 on failure (performs
 * cleanup for failure case). */
static int
regs_sync_resize_allocation(size_t newsz)
{
	if(!shmem_resize(shmem_obj, newsz))
	{
		/* Shared memory ends here. */
		regs_sync_error("Shared memory size exceeded.");
		regs_sync_disable();
		return 0;
	}
	shmem->size_backed = newsz;
	return 1;
}

/* Unlocks mutex that protects shared memory. */
static void
regs_sync_leave_critical_section(void)
{
	shmem->data_is_consistent = 1;
	if(gmux_unlock(shmem_gmux) != 0)
	{
		regs_sync_error("Failed to unlock mutex.");
	}
}

void
regs_sync_from_shared_memory(void)
{
	if(!regs_sync_enter_critical_section())
	{
		return;
	}

	if(shmem->generation != seen_generation && shmem->data_is_consistent)
	{
		/* Other instance changed the register contents, let's check the details. */

		int i;
		for(i = 0; i < NUM_REGISTERS; ++i)
		{
			if(shmem->reg_metadata[i].generation != seen_generation)
			{
				free_string_array(registers[i].files, registers[i].nfiles);

				registers[i].nfiles = shmem->reg_metadata[i].num_entries;
				registers[i].files = reallocarray(NULL, registers[i].nfiles,
						sizeof(char *));

				int j;
				const char *curstrptr = shmem_raw + shmem->reg_metadata[i].offset;
				for(j = 0; j < registers[i].nfiles; ++j)
				{
					size_t curlen = strlen(curstrptr) + 1;
					registers[i].files[j] = malloc(curlen);
					memcpy(registers[i].files[j], curstrptr, curlen);
					curstrptr += curlen;
				}
			}
		}
		seen_generation = shmem->generation;
	}

	regs_sync_leave_critical_section();
}

TSTATIC int
regs_sync_enabled(void)
{
	return (shmem != NULL);
}

TSTATIC void
regs_sync_debug_print_memory(void)
{
	int i;

	printf("-- BEGIN VIFM shared memory synchronization DUMP --\n");
	printf("| local\n");
	for(i = 0; i < NUM_REGISTERS; ++i)
	{
		printf("| | register %2d name=%c, nfiles=%d, files=\n", i,
			registers[i].name, registers[i].nfiles);

		int j;
		for(j = 0; j < registers[i].nfiles; ++j)
		{
			printf("| | | %s\n", registers[i].files[j]);
		}
	}
	printf("| meta shmem=%p, shmem_raw=%p, seen_generation=%u\n",
		shmem, shmem_raw, seen_generation);
	if(regs_sync_enabled())
	{
		printf("| shmem size_backed=%d, generation=%u, length_area_used=%d, "
			"reg_metadata=\n", (int)shmem->size_backed, shmem->generation,
			(int)shmem->length_area_used);
		for(i = 0; i < NUM_REGISTERS; ++i)
		{
			printf(
				"| | register %2d generation=%u, num_entries=%d, "
					"offset=%d, length_used=%d, length_available=%d\n",
				i,
				shmem->reg_metadata[i].generation,
				(int)shmem->reg_metadata[i].num_entries,
				(int)shmem->reg_metadata[i].offset,
				(int)shmem->reg_metadata[i].length_used,
				(int)shmem->reg_metadata[i].length_available
			);
			printf("| | | used  =");
			regs_sync_debug_print_area(shmem->reg_metadata[i].offset,
				shmem->reg_metadata[i].length_used);
			printf("| | | unused=");
			regs_sync_debug_print_area(
				shmem->reg_metadata[i].length_used,
				shmem->reg_metadata[i].length_available -
					shmem->reg_metadata[i].length_used
			);
		}
	}
	printf("-- END   VIFM shared memory synchronization DUMP --\n");
}

/* Dumps region of shared memory to stdout. */
static void
regs_sync_debug_print_area(size_t offset, size_t length)
{
	size_t i;
	for(i = 0; i < length; ++i)
	{
		const char val = shmem_raw[offset + i];
		if(val == 0)
		{
			printf("(0)");
		}
		else
		{
			putchar(val);
		}
	}
	putchar('\n');
}

TSTATIC void
regs_sync_enable_test_mode(void)
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
