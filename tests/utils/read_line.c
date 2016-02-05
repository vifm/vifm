#include <stic.h>

#include <stdio.h> /* FILE fclose() fopen() */
#include <stdlib.h> /* free() malloc() */

#include "../../src/utils/file_streams.h"

TEST(buffer_can_be_null)
{
	FILE *const fp = fopen(TEST_DATA_PATH "/read/very-long-line", "r");
	char *line = NULL;

	if(fp == NULL)
	{
		assert_fail("Failed to open file");
		return;
	}

	line = read_line(fp, line);
	assert_string_equal("# short line", line);

	free(line);
	fclose(fp);
}

TEST(buffer_can_be_not_null)
{
	FILE *const fp = fopen(TEST_DATA_PATH "/read/very-long-line", "r");
	char *line = malloc(10);

	if(fp == NULL)
	{
		assert_fail("Failed to open file");
		free(line);
		return;
	}

	line = read_line(fp, line);
	assert_string_equal("# short line", line);

	free(line);
	fclose(fp);
}

TEST(very_long_line_can_be_read)
{
	FILE *const fp = fopen(TEST_DATA_PATH "/read/very-long-line", "r");
	char *line = NULL;

	if(fp == NULL)
	{
		assert_fail("Failed to open file");
		return;
	}

	line = read_line(fp, line);
	line = read_line(fp, line);
	assert_string_equal("f^vc1gx_05\\.jpg\\.part$|^vb8he_11\\.jpg\\.part$|^vb5hg_"
			"05\\.jpg\\.part$|^va9ha_06\\.jpg\\.part$|^va7hc_08\\.jpg\\.part$|^va3hc_"
			"13\\.jpg\\.part$|^va3hc_01\\.jpg\\.part$|^va1gp_08\\.jpg\\.part$|^v9zgq_"
			"06\\.jpg\\.part$|^v9ygx_08\\.jpg\\.part$|^v9xgk_07\\.jpg\\.part$|^v9tgx_"
			"07\\.jpg\\.part$|^v9sgk_09\\.jpg\\.part$|^v9rge_06\\.jpg\\.part$|^v9nbo_"
			"02\\.jpg\\.part$|^v9kfu_12\\.jpg\\.part$|^v9igs_07\\.jpg\\.part$|^v9hgb_"
			"03\\.jpg\\.part$|^v9ego_06\\.jpg\\.part$|^v9bgm_14\\.jpg\\.part$|^v9bgm_"
			"05\\.jpg\\.part$|^v9agn_09\\.jpg\\.part$|^v8zfp_14\\.jpg\\.part$|^v8zfp_"
			"03\\.jpg\\.part$|^v8udk_11\\.jpg\\.part$|^v8udk_05\\.jpg\\.part$|^v8tgh_"
			"12\\.jpg\\.part$|^v8nbt_08\\.jpg\\.part$|^v8kfc_01\\.jpg\\.part$|^v8ifw_"
			"11\\.jpg\\.part$|^v8hfz_12\\.jpg\\.part$|^v8gfx_12\\.jpg\\.part$|^v8ebj_"
			"04\\.jpg\\.part$|^v8bpr_07\\.jpg\\.part$|^v7yfp_10\\.jpg\\.part$|^v7rfo_"
			"09\\.jpg\\.part$|^v7nfq_09\\.jpg\\.part$|^v7ifk_11\\.jpg\\.part$|^v7ifk_"
			"03\\.jpg\\.part$|^v7hfp_10\\.jpg\\.part$|^v7gfm_05\\.jpg\\.part$|^v7efn_"
			"04\\.jpg\\.part$|^v7bfm_04\\.jpg\\.part$|^v6zex_05\\.jpg\\.part$|^v6xfn_"
			"09\\.jpg\\.part$|^v6xfn_01\\.jpg\\.part$|^v6wfk_01\\.jpg\\.part$|^v6sej_"
			"03\\.jpg\\.part$|^v6oej_05\\.jpg\\.part$|^v6naa_05\\.jpg\\.part$|^v6jex_"
			"03\\.jpg\\.part$|^v6hek_06\\.jpg\\.part$|^v6faj_04\\.jpg\\.part$|^v6dfm_"
			"08\\.jpg\\.part$|^v6cem_06\\.jpg\\.part$|^v6cem_02\\.jpg\\.part$|^v6baz_"
			"08\\.jpg\\.part$|^v5pdg_01\\.jpg\\.part$|^v5mbk_04\\.jpg\\.part$|^v5lek_"
			"04\\.jpg\\.part$|^v5hem_02\\.jpg\\.part$|^v4wen_02\\.jpg\\.part$|^v4rbi_"
			"03\\.jpg\\.part$|^v4mbk_03\\.jpg\\.part$|^v4lam_01\\.jpg\\.part$|^v4jas_"
			"05\\.jpg\\.part$|^v4ibx_02\\.jpg\\.part$|^v4gbr_01\\.jpg\\.part$|^v4dbt_"
			"05\\.jpg\\.part$|^v4cbk_02\\.jpg\\.part$|^v4baq_04\\.jpg\\.part$|^v3yaa_"
			"04\\.jpg\\.part$|^v3xbu_01\\.jpg\\.part$|^v3sbq_02\\.jpg\\.part$|^v3abg_"
			"01\\.jpg\\.part$|^sb7ae_08\\.jpg\\.part$|^sb6hh_09\\.jpg\\.part$|^sb4ha_"
			"08\\.jpg\\.part$|^sb3hg_04\\.jpg\\.part$|^sb1gw_03\\.jpg\\.part$|^sa9fq_"
			"12\\.jpg\\.part$|^sa9fq_01\\.jpg\\.part$|^sa8hd_01\\.jpg\\.part$|^sa6hd_"
			"05\\.jpg\\.part$|^sa4hb_10\\.jpg\\.part$|^sa3gw_09\\.jpg\\.part$|^sa1ep_"
			"13\\.jpg\\.part$|^s9zep_12\\.jpg\\.part$|^s9yfp_02\\.jpg\\.part$|^s9ogt_"
			"05\\.jpg\\.part$|^s9nfe_15\\.jpg\\.part$|^s9nfe_02\\.jpg\\.part$|^s9mgs_"
			"02\\.jpg\\.part$|^s9hfp_04\\.jpg\\.part$|^s9gas_05\\.jpg\\.part$|^s9fbo_"
			"03\\.jpg\\.part$|^s9efw_09\\.jpg\\.part$|^s8xgl_14\\.jpg\\.part$|^s8wbg_"
			"04\\.jpg\\.part$|^s8tag_04\\.jpg\\.part$|^s8qgb_06\\.jpg\\.part$|^s8pgf_"
			"06\\.jpg\\.part$|^s8ngv_12\\.jpg\\.part$|^s8lgc_14\\.jpg\\.part$|^s8lgc_"
			"02\\.jpg\\.part$|^s8hfp_01\\.jpg\\.part$|^s8gga_03\\.jpg\\.part$|^s8efx_"
			"13\\.jpg\\.part$|^s8bfw_02\\.jpg\\.part$|^s7ybh_03\\.jpg\\.part$|^s7xfu_"
			"10\\.jpg\\.part$|^s7xfu_02\\.jpg\\.part$|^s7ubk_03\\.jpg\\.part$|^s7tfo_"
			"11\\.jpg\\.part$|^s7tfo_01\\.jpg\\.part$|^s7qpr_09\\.jpg\\.part$|^s7odf_"
			"02\\.jpg\\.part$|^s7lfr_08\\.jpg\\.part$|^s7kfr_06\\.jpg\\.part$|^s7eat_"
			"02\\.jpg\\.part$|^s7abh_06\\.jpg\\.part$|^s6ubk_01\\.jpg\\.part$|^s6raj_"
			"01\\.jpg\\.part$|^s6lbz_04\\.jpg\\.part$|^s6kwp_03\\.jpg\\.part$|^s6aej_"
			"02\\.jpg\\.part$|^s5yaj_02\\.jpg\\.part$|^s5weq_02\\.jpg\\.part$|^s5twp_"
			"03\\.jpg\\.part$|^s5caj_01\\.jpg\\.part$|^s4wej_02\\.jpg\\.part$|^s4pbo_"
			"01\\.jpg\\.part$|^s4ipc_01\\.jpg\\.part$|^s4esm_01\\.jpg\\.part$|^s3vbk_"
			"01\\.jpg\\.part$|^s3bbi_01\\.jpg\\.part$|^s2mas_01\\.jpg\\.jpg\\.part$|^"
			"s1dsi_01\\.jpg\\.part$|^lb9aq_07\\.jpg\\.part$|^lb6gz_10\\.jpg\\.part$|^"
			"lb5ah_11\\.jpg\\.part$|^lb2gn_05\\.jpg\\.part$|^lb1go_11\\.jpg\\.part$|^"
			"la9hg_08\\.jpg\\.part$|^la5hc_08\\.jpg\\.part$|^la4gx_11\\.jpg\\.part$|^"
			"la3gs_03\\.jpg\\.part$|^l53as_04\\.jpg\\.part$|^l9wep_14\\.jpg\\.part$|^"
			"l9vgx_13\\.jpg\\.part$|^l9ugh_11\\.jpg\\.part$|^l9trn_12\\.jpg\\.part$|^"
			"l9pfq_08\\.jpg\\.part$|^l9ogp_12\\.jpg\\.part$|^l9lff_08\\.jpg\\.part$|^"
			"l9kgp_09\\.jpg\\.part$|^l9kgp_03\\.jpg\\.part$|^l9hgm_01\\.jpg\\.part$|^"
			"l9ggp_07\\.jpg\\.part$|^l9fgl_02\\.jpg\\.part$|^l9cdk_11\\.jpg\\.part$|^"
			"l9cdk_02\\.jpg\\.part$|^l8vfj_06\\.jpg\\.part$|^l8sfp_10\\.jpg\\.part$|^"
			"l8rgh_08\\.jpg\\.part$|^l8qam_04\\.jpg\\.part$|^l8oet_01\\.jpg\\.part$|^"
			"l8nek_09\\.jpg\\.part$|^l8lfk_03\\.jpg\\.part$|^l8jfu_10\\.jpg\\.part$|^"
			"l8ifk_13\\.jpg\\.part$|^l8hgb_12\\.jpg\\.part$|^l8dfw_13\\.jpg\\.part$|^"
			"l8cae_08\\.jpg\\.part$|^l8bel_05\\.jpg\\.part$|^l8afp_13\\.jpg\\.part$|^"
			"l8afp_03\\.jpg\\.part$|^l7yfm_13\\.jpg\\.part$|^l7yfm_01\\.jpg\\.part$|^"
			"l7wfo_11\\.jpg\\.part$|^l7tfl_01\\.jpg\\.part$|^l7qfo_11\\.jpg\\.part$|^"
			"l7pbj_06\\.jpg\\.part$|^l7ofn_09\\.jpg\\.part$|^l7jam_01\\.jpg\\.part$|^"
			"l7gat_10\\.jpg\\.part$|^l7fsq_13\\.jpg\\.part$|^l7eas_05\\.jpg\\.part$|^"
			"l7dak_04\\.jpg\\.part$|^l7cbi_01\\.jpg\\.part$|^l6yfj_01\\.jpg\\.part$|^"
			"l6wdb_03\\.jpg\\.part$|^l6leg_02\\.jpg\\.part$|^l6iel_05\\.jpg\\.part$|^"
			"l6has_06\\.jpg\\.part$|^l6gbv_01\\.jpg\\.part$|^l5yeq_03\\.jpg\\.part$|^"
			"l5weg_06\\.jpg\\.part$|^l5res_02\\.jpg\\.part$|^l5qbh_01\\.jpg\\.part$|^"
			"l5oeg_01\\.jpg\\.part$|^l5nbu_02\\.jpg\\.part$|^l5ldf_03\\.jpg\\.part$|^"
			"l5kbg_01\\.jpg\\.part$|^l5faz_03\\.jpg\\.part$|^l5cbh_02\\.jpg\\.part$|^"
			"l5bbu_02\\.jpg\\.part$|^l4xam_02\\.jpg\\.part$|^l4vbr_02\\.jpg\\.part$|^"
			"l4oag_01\\.jpg\\.part$|^l4jas_06\\.jpg\\.part$|^l4gaz_02\\.jpg\\.part$|^"
			"l4eag_08\\.jpg\\.part$|^l4dbt_02\\.jpg\\.part$|^l4baq_12\\.jpg\\.part$|^"
			"l3vbk_02\\.jpg\\.part$|^l3upc_03\\.jpg\\.part$|^l3rds_09\\.jpg\\.part$|^"
			"l3rds_04\\.jpg\\.part$|^l3qpc_10\\.jpg\\.part$|^l3qpc_01\\.jpg\\.part$|^"
			"l3obs_11\\.jpg\\.part$|^l3nat_10\\.jpg\\.part$|^l3mbh_08\\.jpg\\.part$|^"
			"l3kds_03\\.jpg\\.part$|^l3abg_04\\.jpg\\.part$|^l2yae_06\\.jpg\\.part$|^"
			"l2saq_08\\.jpg\\.part$|^l2qag_03\\.jpg\\.part$|^l2oac_05\\.jpg\\.part$|^"
			"l2iad_08\\.jpg\\.part$|^l1gsf_04\\.jpg\\.part$|^l1asl_04\\.jpg\\.part$|^"
			"jc1db_03\\.jpg\\.part$|^jb9gm_08\\.jpg\\.part$|^jb6hf_08\\.jpg\\.part$|^"
			"jb3fp_12\\.jpg\\.part$|^jb2hh_02\\.jpg\\.part$|^jb1gm_01\\.jpg\\.part$",
			line);

	free(line);
	fclose(fp);
}

TEST(empty_line_is_read_correctly)
{
	FILE *const fp = fopen(TEST_DATA_PATH "/read/very-long-line", "r");
	char *line = NULL;

	if(fp == NULL)
	{
		assert_fail("Failed to open file");
		return;
	}

	line = read_line(fp, line);
	line = read_line(fp, line);
	line = read_line(fp, line);
	assert_string_equal("", line);

	free(line);
	fclose(fp);
}

TEST(next_to_empty_line_is_read_correctly)
{
	FILE *const fp = fopen(TEST_DATA_PATH "/read/very-long-line", "r");
	char *line = NULL;

	if(fp == NULL)
	{
		assert_fail("Failed to open file");
		return;
	}

	line = read_line(fp, line);
	line = read_line(fp, line);
	line = read_line(fp, line);
	line = read_line(fp, line);
	assert_string_equal("# the line above is empty", line);

	free(line);
	fclose(fp);
}

TEST(the_last_line_is_read_correctly)
{
	FILE *const fp = fopen(TEST_DATA_PATH "/read/very-long-line", "r");
	char *line = NULL;

	if(fp == NULL)
	{
		assert_fail("Failed to open file");
		return;
	}

	line = read_line(fp, line);
	line = read_line(fp, line);
	line = read_line(fp, line);
	line = read_line(fp, line);
	line = read_line(fp, line);
	assert_string_equal("# this is the last file", line);

	free(line);
	fclose(fp);
}

TEST(reads_after_eof_return_null)
{
	const int READ_COUNT = 10;
	FILE *const fp = fopen(TEST_DATA_PATH "/read/very-long-line", "r");
	char *line = NULL;
	int i;

	if(fp == NULL)
	{
		assert_fail("Failed to open file");
		return;
	}

	line = read_line(fp, line);
	line = read_line(fp, line);
	line = read_line(fp, line);
	line = read_line(fp, line);
	line = read_line(fp, line);
	assert_false(line == NULL);
	for(i = 0; i < READ_COUNT; i++)
	{
		line = read_line(fp, line);
		assert_string_equal(NULL, line);
	}

	free(line);
	fclose(fp);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
