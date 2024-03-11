#include <stdio.h> /* EOF FILE fclose() fflush() fgets() */
#include <string.h> /* strcmp() strlen() strspn() */

/*
 * Usage: io_tester modes
 *
 * `modes` must be a three-letter sequence of:
 *  * c -- close
 *  * k -- keep
 *  * u -- use
 *
 * A letter corresponds to stdin, stdout and stderr in this order.
 *
 * Example modes: kcc, cuc, ccu
 *
 * When a file descriptor is being used, stdin expects "stdin" string and
 * stdout and stderr print "stdout" and "stderr" respectively.
 */

int
main(int argc, char *argv[])
{
	if(argc != 2)
	{
		return 10;
	}

	const char *modes = argv[1];
	if(strlen(modes) != 3)
	{
		return 11;
	}
	if(strspn(modes, "cku") != 3)
	{
		return 12;
	}

	FILE *fps[3] = { stdin, stdout, stderr };
	const char *data[3] = { "stdin", "stdout", "stderr" };
	int i;

	/* Start by closing descriptors to make sure others can still be used. */
	for(i = 0; i < 3; ++i)
	{
		if(modes[i] == 'c')
		{
			if(fclose(fps[i]) != 0)
			{
				return 20 + i;
			}
		}
	}

	for(i = 0; i < 3; ++i)
	{
		if(modes[i] != 'u')
		{
			continue;
		}

		if(i == 0)
		{
			char buf[16];
			if(fgets(buf, sizeof(buf), fps[i]) != buf)
			{
				return 30;
			}
			if(strcmp(buf, data[i]) != 0)
			{
				return 31;
			}
		}
		else
		{
			if(fputs(data[i], fps[i]) == EOF)
			{
				return 40 + i;
			}
			if(fflush(fps[i]) == EOF)
			{
				return 50 + i;
			}
		}
	}

	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
