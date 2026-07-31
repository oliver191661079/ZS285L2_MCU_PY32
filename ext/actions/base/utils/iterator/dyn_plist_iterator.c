#include <zephyr.h>
#include <fs.h>
#include <disk_access.h>
#include <mem_manager.h>
#include <logging/sys_log.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <iterator/file_iterator.h>
#include <fs_manager.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_DIR_LEVEL 9
#define FULL_PATH_LEN (MAX_URL_LEN + 2)
#define OPEN_MODE "bycluster:"
#define CLUSTER "cluster:"
#define MAX_SUPPORT_FILE_CNT CONFIG_ITERATOR_SUPPORT_FILE_CNT
#define FAT16_FAR_CLUST 0xfffffff1

#define PLAY_LIST_CONTAIN_MAX_CNT (41)
#define PLAY_LIST_PREV_END_MAX (PLAY_LIST_CONTAIN_MAX_CNT/2)
#define PLAY_LIST_NEXT_END_MAX (PLAY_LIST_CONTAIN_MAX_CNT/2)
#define ITERATOR_CONTAIN_MAX_CNT (PLAY_LIST_CONTAIN_MAX_CNT/2 + 1)


//#define DYN_PLIST_DEBUG

struct file_info_t{
	uint32_t file_cluster;	/*file cluster for fs_open_cluster*/
	uint16_t file_num_ofs;	/*file blk_ofs for fs_open_cluster*/
};

struct play_list_t {
	struct  file_info_t file_info[PLAY_LIST_CONTAIN_MAX_CNT];
	uint16_t sum_file_count;	/*sum valid files in disk*/
	uint16_t file_seq_num;		/*file in sum file sequence number*/
	uint8_t cur_plist_id;		/*current file playing id in file info array*/
	uint8_t prev_sum;		/*file number of prev info left in playlist*/
	uint8_t next_sum;		/*file number of prev info next in playlist*/

	uint16_t sum_folder_count;	/*sum valid folder in disk,start from 0*/
	uint8_t mode;				/*play mode 0--Full cycle;1--single cycle;2--full,no cycle;3--folder cycle*/
	const char *topdir;
	int (*match_fn)(const char *path, int is_dir);

	char * scaning_full_path;
	uint8_t inited:1;
	uint8_t scaned:1;
	uint8_t no_bp_start:1;
	uint8_t found_cursor:1;
	uint8_t is_file_type:1;
	uint8_t has_sub_folder:1;
};

static struct play_list_t *play_list = NULL;

struct file_iterator_data {

	struct  file_info_t file_info[ITERATOR_CONTAIN_MAX_CNT];
	struct  file_info_t start_file_info[PLAY_LIST_NEXT_END_MAX>>1];

	uint32_t cluster;
	uint32_t cluster_file_seq;

	/* cached dir structures */
	s16_t level;
	uint16_t max_level;
	fs_dir_t *dirs[MAX_DIR_LEVEL];

	/* used to compose new full path */
	uint16_t *dname_len; /* used to store name len of every level directories */
	uint16_t fname_len;  /* name len of current file */
	uint16_t full_len;   /* name len of full path */
	char *full_path;  /* store the full path */

	struct file_iterator_cursor cursor;

	int (*match_fn)(const char *path, int is_dir);

	struct fs_dirent *dirent;
};

static struct play_list_t *get_play_list(void)
{
	return play_list;
}

static void dump_list_info(const char * s)
{
	SYS_LOG_INF("%s list_info: sum file %d, seq %d pre %d, next %d, cur %d\n",s, play_list->sum_file_count,  play_list->file_seq_num,
			play_list->prev_sum, play_list->next_sum, play_list->cur_plist_id );
	SYS_LOG_INF("file cluster %d, %d\n", play_list->file_info[play_list->cur_plist_id].file_cluster, play_list->file_info[play_list->cur_plist_id].file_num_ofs);

}

#if 0
static void dump_plist_file_info(void)
{
	SYS_LOG_INF("dump plist\n");
	for(u32_t n = 0; n < PLAY_LIST_CONTAIN_MAX_CNT && n < plist->sum_file_count; n++)
	{
		SYS_LOG_INF("id=%d, cluster %d cluster_file_seq %d\n", n, plist->file_info[n].file_cluster, plist->file_info[n].file_num_ofs);
	}
}
#endif



static int file_iterator_get_plist_info(struct iterator *iter, void *param)
{
	*(uint16_t *)param = play_list->sum_file_count;
	if(play_list->scaned)
		return 0;
	else
		return -1;
}

static void dyn_prepare_upate_scan(struct iterator *iter);

int file_iterator_need_update_list(struct iterator *iter)
{
	struct file_iterator_data *data = iter->data;
	struct play_list_t *plist = get_play_list();
	if(!data || !play_list)
		return 0;

	if(!plist->scaned || plist->sum_file_count <= PLAY_LIST_CONTAIN_MAX_CNT)
		return 0;


	if(plist->prev_sum < (PLAY_LIST_CONTAIN_MAX_CNT >>2))
	{
		goto update;
	}

	if(plist->next_sum < (PLAY_LIST_CONTAIN_MAX_CNT >>2))
	{
		goto update;
	}

	return 0;

update:
	plist->scaned = 0;
	plist->sum_file_count = 0;
	plist->sum_folder_count = 0;
	plist->found_cursor = 0;
	dyn_prepare_upate_scan(iter);
	return 1;

}

static int file_iterator_has_found_cursor(struct iterator *iter)
{
	struct play_list_t *plist = get_play_list();

	if(plist)
		return (int)(plist->found_cursor || plist->scaned);
	else
		return 0;
}

static int file_iterator_destroy(struct iterator *iter)
{
	struct file_iterator_data *data = iter->data;
	int i;


	if(!play_list->scaned && play_list->inited)
	{
		for (i = data->level; i >= 0; i--)
			fs_closedir(data->dirs[i]);
	}

	if (data->dirent)
		mem_free(data->dirent);

	for (i = 0; i < data->max_level; i++) {
		if (data->dirs[i]) {
			mem_free(data->dirs[i]);
		}
	}
	if (data->dname_len)
		mem_free(data->dname_len);

	if (data->full_path)
		mem_free(data->full_path);

	mem_free(data);

	if (play_list) {
		mem_free(play_list);
		play_list = NULL;
	}
	return 0;
}


static void calc_next_playlist_info(struct play_list_t *plist, uint8_t add)
{

	if (add) {
		if(plist->next_sum){
			plist->next_sum --;
			plist->prev_sum ++;

			if(plist->cur_plist_id < (PLAY_LIST_CONTAIN_MAX_CNT -1)){
				plist->cur_plist_id++;
			}else{
				plist->cur_plist_id = 0;
			}
		}else{
			if(plist->sum_file_count <= PLAY_LIST_CONTAIN_MAX_CNT)
			{
				plist->cur_plist_id = ( PLAY_LIST_CONTAIN_MAX_CNT + plist->cur_plist_id + (PLAY_LIST_CONTAIN_MAX_CNT - plist->prev_sum))%PLAY_LIST_CONTAIN_MAX_CNT;
				plist->next_sum = plist->sum_file_count - 1;
				plist->prev_sum = 0;

			}else{
				//todo start to search more next
			}
		}

		plist->file_seq_num++;
		if(plist->file_seq_num > plist->sum_file_count)
		{
			plist->file_seq_num = 1;
		}


	} else {

		if(plist->prev_sum){
			plist->prev_sum --;
			plist->next_sum ++;


			if(plist->cur_plist_id){
				plist->cur_plist_id --;
			}else{
				plist->cur_plist_id = (PLAY_LIST_CONTAIN_MAX_CNT -1);
			}
		}else{
			if(plist->sum_file_count <= PLAY_LIST_CONTAIN_MAX_CNT)
			{
				plist->cur_plist_id = ( PLAY_LIST_CONTAIN_MAX_CNT + plist->cur_plist_id - (PLAY_LIST_CONTAIN_MAX_CNT - plist->next_sum))%PLAY_LIST_CONTAIN_MAX_CNT;
				plist->prev_sum = plist->sum_file_count - 1;
				plist->next_sum = 0;
			}else{
				//todo start to search more pre
			}
		}
		if (plist->file_seq_num > 1){
			plist->file_seq_num--;
		}else{
			plist->file_seq_num = plist->sum_file_count;
		}
	}

	if(plist->sum_file_count > PLAY_LIST_CONTAIN_MAX_CNT)
	{
		if(plist->next_sum > PLAY_LIST_PREV_END_MAX)
		{
			plist->next_sum = PLAY_LIST_PREV_END_MAX;
		}

		if(plist->prev_sum > PLAY_LIST_NEXT_END_MAX)
		{
			plist->prev_sum = PLAY_LIST_NEXT_END_MAX;
		}
	}
}

//source:SN60~B.MP3/SNA0~ROOTDI~1/,dest=SD:
//SD:SNA0~ROOTDI~1/SN60~B.MP3
/*url:bycluster:/SD://cluster:2/320/12992420/.mp3*/
static void _file_format_get(char *name, char **file_format)
{
	int len = strlen(name);
	while (len) {
		if (name[--len] == '.') {
			*file_format = name + len;
			break;
		}
	}
}
static int file_dirname_get(struct play_list_t *plist, struct iterator *iter)
{
	struct file_iterator_data *data = iter->data;
	int res = -ENOENT;
	uint16_t times = 0;
	DIR* dp = NULL;
	char *file_format = NULL;
	fs_dir_t *zdp = mem_malloc(sizeof(fs_dir_t));
	struct fs_dirent *entry = mem_malloc(sizeof(struct fs_dirent));

	if (!zdp || !entry)
		goto exit;

	/*read file name*/
	dp = &(zdp->dp);

	u32_t cluster = plist->file_info[plist->cur_plist_id].file_cluster;
	u32_t blk_ofs = plist->file_info[plist->cur_plist_id].file_num_ofs;

	//printk("%s cur id %d,  file_cluster %d, blk_ofs %d\n",__func__, plist->cur_plist_id, cluster, blk_ofs);
	res = fs_opendir_cluster(zdp, plist->topdir, cluster, 0);

	if (!res) {
		do {
			memset(entry, 0, sizeof(struct fs_dirent));
			res = fs_readdir(zdp, entry);
			if (res || entry->name[0] == 0) {
				SYS_LOG_ERR("fs_readdir failed (res=%d), skip this\n", res);
				fs_closedir(zdp);
				goto exit;
			}
			/* filter out unmatch directory or file */
			if (plist->match_fn && entry->type == FS_DIR_ENTRY_FILE && plist->match_fn(entry->name, 0))
			{
				times++;
				//printk("xxblk_ofs %d name %s\n",(u32_t)dp->blk_ofs, entry->name);
			}

		} while (times < blk_ofs);
		SYS_LOG_INF("file name:%s\n", entry->name);
		fs_closedir(zdp);
	} else {
		SYS_LOG_ERR("fs_opendir failed (res=%d)\n", res);
		fs_closedir(zdp);
		goto exit;
	}
	/* get file format */
	_file_format_get(entry->name, &file_format);
	/*get file path*/
	memset(data->full_path, 0, FULL_PATH_LEN);
	snprintf(data->full_path, FULL_PATH_LEN, "%s%s/%s%u/%lu/%u/%s", OPEN_MODE, plist->topdir, CLUSTER,
		cluster, dp->blk_ofs, entry->size, file_format);

	data->cursor.path = data->full_path;
	iter->cursor = &data->cursor;

	SYS_LOG_INF("full_path:%s\n", data->full_path);

exit:
	if(zdp)
		mem_free(zdp);
	if(entry)
		mem_free(entry);
	return res;

}

static int file_iterator_set_mode(struct iterator *iter, uint8_t mode)
{
	struct play_list_t *plist = get_play_list();

	if (!plist)
		return -ENXIO;

	plist->mode = mode;

	return 0;
}


static const void *next(struct iterator *iter, bool force_switch, uint16_t *track_no)
{
	struct file_iterator_data *data = iter->data;
	struct play_list_t *plist = get_play_list();

	if (!plist || !plist->sum_file_count)
		return NULL;
	/*no cycle in mode 2*/
	if (plist->mode == 2 && plist->file_seq_num == plist->sum_file_count && !force_switch)
		return NULL;

	calc_next_playlist_info(plist, 1);

	if (file_dirname_get(plist, iter))
	{

		if (track_no)
			*(uint16_t *)track_no = MAX_SUPPORT_FILE_CNT +1;

		printk("disk maybe has plug-out %d\n", __LINE__);
		return NULL;
	}

	if (track_no)
		*(uint16_t *)track_no = plist->file_seq_num;

	dump_list_info(__func__);
	return data->full_path;
}

//if track_no is 0, not found song, still keep searching.
static const void *file_iterator_next(struct iterator *iter, bool force_switch, uint16_t *track_no)
{
	struct file_iterator_data *data = iter->data;
	struct play_list_t *plist = get_play_list();

	if (!plist)
		return NULL;

	if(plist->scaned)
	{
		if(!plist->sum_file_count)
			return NULL;

		if(plist->no_bp_start)
		{
			plist->no_bp_start = 0;

			if (file_dirname_get(plist, iter))
			{
				if (track_no)
					*(uint16_t *)track_no = MAX_SUPPORT_FILE_CNT +1;

				printk("disk maybe has plug-out %d\n", __LINE__);
				return NULL;
			}

			if (track_no)
			{
				*(uint16_t *)track_no = plist->file_seq_num;
			}

			return data->full_path;
		}

		return next(iter, force_switch, track_no);
	}else{
		if((plist->sum_file_count > plist->file_seq_num)  && plist->found_cursor){

			if(!plist->no_bp_start)
			{
				calc_next_playlist_info(plist, 1);
			}

			plist->no_bp_start = 0;

			if (file_dirname_get(plist, iter))
			{
				if (track_no)
					*(uint16_t *)track_no = MAX_SUPPORT_FILE_CNT +1;

				printk("disk maybe has plug-out %d\n", __LINE__);

				return NULL;
			}

			dump_list_info("next");

		}else{
			*(uint16_t *)track_no = 0;
			return NULL;
		}

		if (track_no)
		{
			*(uint16_t *)track_no = plist->file_seq_num;
		}

		return data->full_path;
	}

}

static const void *prev(struct iterator *iter, uint16_t *track_no)
{
	struct file_iterator_data *data = iter->data;
	struct play_list_t *plist = get_play_list();

	if (!plist || !plist->sum_file_count)
		return NULL;

	calc_next_playlist_info(plist, 0);

	if (file_dirname_get(plist, iter))
	{
		if (track_no)
			*(uint16_t *)track_no = MAX_SUPPORT_FILE_CNT +1;

		printk("disk maybe has plug-out %d\n", __LINE__);
		return NULL;
	}

	if (track_no)
		*(uint16_t *)track_no = plist->file_seq_num;

	dump_list_info(__func__);
	return data->full_path;
}


static const void *file_iterator_prev(struct iterator *iter, uint16_t *track_no)
{
	struct file_iterator_data *data = iter->data;
	struct play_list_t *plist = get_play_list();

	if (!plist)
		return NULL;

	if(plist->scaned)
	{
		if(!plist->sum_file_count)
			return NULL;

		return prev(iter, track_no);

	}else{
		if((plist->file_seq_num > 1)  && plist->found_cursor){

			if(!plist->no_bp_start)
				calc_next_playlist_info(plist, 0);

			plist->no_bp_start = 0;

			if (file_dirname_get(plist, iter))
			{
				if (track_no)
					*(uint16_t *)track_no = MAX_SUPPORT_FILE_CNT +1;

				printk("disk maybe has plug-out %d\n", __LINE__);
				return NULL;
			}

			dump_list_info("prev");

		}else{
			*(uint16_t *)track_no = 0;
			return NULL;
		}

		if (track_no)
		{
			*(uint16_t *)track_no = plist->file_seq_num;
		}

		return data->full_path;
	}

}

static int _back_to_topdir(struct file_iterator_data *data)
{
	int res = 0;
	int i;
	DIR* dp = NULL;

	/* close all sub-directories, and reset topdir state */
	for (i = data->level; i >= 0; i--)
		fs_closedir(data->dirs[i]);

	data->level = -1;
	data->fname_len = 0;
	data->full_len = data->dname_len[0];
	data->full_path[data->full_len] = 0;

	if (data->full_path[data->full_len - 1] == '/')
		data->full_path[data->full_len - 1] = 0;

	res = fs_opendir(data->dirs[0], data->full_path);
	if (res) {
		SYS_LOG_ERR("fs_opendir %s failed (res=%d)\n", data->full_path, res);
		return res;
	}
	if (data->full_path[data->full_len - 1] != ':')
		data->full_path[data->full_len - 1] = '/';

	data->level = 0;

	dp = &(data->dirs[data->level]->dp);
	data->cluster = dp->clust;

	return 0;
}

static int file_iterator_playlist_init(struct play_list_t *plist, struct file_iterator_data *data, const void *param)
{
	int res = -ENOENT;
	const struct file_iterator_param *iter_param = (struct file_iterator_param *)param;
	fs_dir_t *zdp = NULL;
	DIR* dp = NULL;

	zdp = mem_malloc(sizeof(fs_dir_t));
	if (!zdp)
		return res;

	if (data->full_path[data->full_len - 1] == '/')
		data->full_path[data->full_len - 1] = 0;
	res = fs_opendir(zdp, data->full_path);
	if (res) {
		SYS_LOG_ERR("fs_opendir %s failed (res=%d)\n", data->full_path, res);
		goto exit;
	}
	if (data->full_path[data->full_len - 1] != ':')
		data->full_path[data->full_len - 1] = '/';

	dp = &zdp->dp;

	plist->topdir = iter_param->topdir;
	plist->match_fn = iter_param->match_fn;

	SYS_LOG_INF("fs info %s\n", data->full_path);
	fs_closedir(zdp);

exit:
	if (zdp)
		mem_free(zdp);
	return res;
}

static void dyn_prepare_upate_scan(struct iterator *iter)
{
	struct file_iterator_data *data = iter->data;
	struct play_list_t *plist = get_play_list();

	strcpy(data->full_path, plist->topdir);
	data->dname_len[0] = strlen(plist->topdir);
	data->full_len = data->dname_len[0];
	if (data->full_path[data->full_len - 1] != ':' &&
		data->full_path[data->full_len - 1] != '/') {
		data->full_path[data->full_len++] = '/';
		data->dname_len[0]++;
	}

	int res = _back_to_topdir(data);
	if (res)
	{
		SYS_LOG_ERR("prepare failed\n");
		return ;
	}

	plist->is_file_type = 1;
	plist->has_sub_folder = 0;

	data->cluster = 0;
	data->cluster_file_seq = 0;

	if(plist->scaning_full_path)
		strcpy(plist->scaning_full_path, data->full_path);

	SYS_LOG_INF("prepare successfully\n");
}


/*url:bycluster:SD:/cluster:2/320/12992420*/
static int get_cursor_info(const char *path, uint32_t *cluster, uint32_t *blk_ofs, uint32_t *file_size)
{
	char *str = NULL;
	char *clust = NULL;
	char *blk = NULL;
	char *temp_url = NULL;
	int res = -EINVAL;

	temp_url = mem_malloc(strlen(path) + 1);
	if (!temp_url)
		goto exit;

	strcpy(temp_url, path);
	str = strstr(temp_url,"/cluster:");
	if (!str)
		goto exit;

	str += strlen("/cluster:");
	clust = str;
	str = strchr(str, '/');
	if (!str)
		goto exit;
	str[0] = 0;

	str++;
	blk = str;
	str = strchr(str, '/');
	if (!str)
		goto exit;
	str[0] = 0;
	str++;

	*cluster = atoi(clust);
	*blk_ofs = atoi(blk);
	*file_size = atoi(str);

	SYS_LOG_DBG("cluster=%u,blk_ofs=%u,file_size=%u\n", *cluster, *blk_ofs, *file_size);
	res = 0;

exit:
	if (temp_url)
		mem_free(temp_url);
	return res;

}


__in_section_unique(file_selector_bss) static char scaning_temp_path[FULL_PATH_LEN];
static char *file_iterator_scan_disk(struct iterator *iter, void *param)
{
	struct file_iterator_data *data = iter->data;
	const struct file_iterator_param *iter_param = (struct file_iterator_param *)param;
	const struct file_iterator_cursor *cursor = iter_param->cursor;
	int res = -ENOENT;
	uint16_t len;
	uint32_t cursor_cluster = 0;
	uint32_t cursor_blk_ofs = 0;
	uint32_t cursor_file_size = 0;
	uint32_t temp_cluster = 0;
	DIR* dp = NULL;
	struct play_list_t *plist = get_play_list();

	iter->cursor = NULL;
	if (data->level < 0){
		plist->scaned = 1;
		goto exit;
	}


	if(!plist->scaning_full_path)
	{
		plist->scaning_full_path = scaning_temp_path;
	}else{
		memcpy(data->full_path, plist->scaning_full_path, FULL_PATH_LEN);
	}

	uint32_t begin = k_cycle_get_32();

	if (cursor->path && !plist->found_cursor){
		SYS_LOG_INF("play cursor %s\n", cursor->path);
		get_cursor_info(cursor->path, &cursor_cluster, &cursor_blk_ofs, &cursor_file_size);
#ifdef DYN_PLIST_DEBUG
		printk("cur url %s\n", cursor->path);
		printk(" cluster %d, ofs %d, size %d\n", cursor_cluster, cursor_blk_ofs, cursor_file_size);
#endif
	}

	do {
		u32_t timeout = 10;
		u32_t diff_ms  = (k_cycle_get_32() - begin)/24/1000;

		dp = &(data->dirs[data->level]->dp);
		temp_cluster = dp->clust;

		if(diff_ms > timeout)
		{
			if(plist->scaning_full_path)
			{
				memcpy(plist->scaning_full_path, data->full_path, FULL_PATH_LEN);
			}

			if(diff_ms > timeout << 1)
			{
				SYS_LOG_WRN("scan %d ms, larger than %dms\n", diff_ms, timeout<<1);
			}
			break;
		}

		res = fs_readdir(data->dirs[data->level], data->dirent);
		if (res)
			SYS_LOG_ERR("fs_readdir failed (res=%d), skip this\n", res);

		/*
		 * if fs_readdir failed or end-of-dir, skip this directory and
		 * go up one level.
		 *
		 * name[0] == 0 means end-of-dir
		 */

		if (res || data->dirent->name[0] == 0) {
			fs_closedir(data->dirs[data->level]);

			if (data->level == 0 && (plist->is_file_type == 0 || plist->has_sub_folder == 0)) {
				data->level = -1;
				plist->scaned = 1;
				goto exit;
			}

			if (plist->is_file_type && plist->has_sub_folder) {
				plist->is_file_type = 0;
				plist->has_sub_folder = 0;
				/*remove file name*/
				data->full_len -= data->fname_len;
				data->full_path[data->full_len] = 0;
				data->fname_len = 0;
				if (data->full_path[data->full_len - 1] == '/')
					data->full_path[data->full_len - 1] = 0;
				SYS_LOG_DBG("repeat_opendir %s \n", data->full_path);
				res = fs_opendir(data->dirs[data->level], data->full_path);
				if (res) {
					SYS_LOG_WRN("repeat_opendir %s failed (res=%d)\n", data->full_path, res);
				}
				if (data->full_path[data->full_len - 1] == 0)
					data->full_path[data->full_len - 1] = '/';
				continue;
			}
			/* up one level to read dir*/
			data->full_len -= data->fname_len + data->dname_len[data->level];
			data->full_path[data->full_len] = 0;
			data->fname_len = 0;
			data->level--;
			SYS_LOG_DBG("up to dir %s\n", data->full_path);
			plist->is_file_type = 0;
			continue;
		}

		/* FIXME: skip hidden diretory or file ? */
		if (data->dirent->name[0] == '.')
			continue;

		/* manage the full path */
		len = (uint16_t)strlen(data->dirent->name);
		data->full_len -= data->fname_len;
		if (data->full_len + len >= FULL_PATH_LEN - 1) {
			SYS_LOG_WRN("too long path, exceed %u bytes (%s)\n",
					data->full_len + len - sizeof(data->full_path), data->dirent->name);
			data->full_len += data->fname_len;
			continue;
		}

		memcpy(&data->full_path[data->full_len], data->dirent->name, len);
		data->fname_len = len;
		data->full_len += len;
		data->full_path[data->full_len] = 0;

		/* filter out unmatch directory or file */
		if (data->match_fn && !data->match_fn(data->full_path,
					data->dirent->type == FS_DIR_ENTRY_DIR)) {
			SYS_LOG_DBG("filter out %s\n", data->full_path);
			continue;
		}

		/* This is a file, file count ++ in file type mode */
		if (data->dirent->type == FS_DIR_ENTRY_FILE && plist->is_file_type) {

			s16_t iter_id = plist->sum_file_count % ITERATOR_CONTAIN_MAX_CNT;

			dp = &(data->dirs[data->level]->dp);
			/*
			 * 播放列表是使用某一簇号打开目录，之后读曲目录，发现符合要求的文件的序号，作为此歌曲
			 * 的播放列表的信息。
			 * 生成播放列表时，读取目录后找到需要写入播放列表的文件，判断簇号是否发生改变。
			 *		如果改变了,需要使用新簇号定位该文件。
			 */
			if(dp->clust != data->cluster)
			{
				SYS_LOG_INF("reopen from %d to %d \n\n", data->cluster, (u32_t)dp->clust);

				temp_cluster = dp->clust;
				data->cluster = temp_cluster;
				data->cluster_file_seq = 0;

				fs_closedir(data->dirs[data->level]);
				res = fs_opendir_cluster(data->dirs[data->level], plist->topdir, temp_cluster, 0);
				if (res)
					SYS_LOG_ERR("open failed (res=%d), skip this\n", res);

				struct fs_dirent *entry = mem_malloc(sizeof(struct fs_dirent));

				do {
					memset(entry, 0, sizeof(struct fs_dirent));
					res = fs_readdir(data->dirs[data->level], entry);
					if (res || entry->name[0] == 0) {
						SYS_LOG_ERR("fs_readdir failed (res=%d), skip this\n", res);
						mem_free(entry);
						entry = NULL;
						break;
					}
					/* filter out unmatch directory or file */
					if (plist->match_fn && entry->type == FS_DIR_ENTRY_FILE && plist->match_fn(entry->name, 0))
					{
						/*
						 * 当前满足条件的文件，不一定是此簇号打开后满足条件的第一个文件。
						 */
						data->cluster_file_seq ++;
						if(data->dirent->size == entry->size)
						{
							data->cluster_file_seq --;
							mem_free(entry);
							entry = NULL;
							break;
						}
					}
				} while (1);
			}

			data->cluster_file_seq ++;

			data->file_info[iter_id].file_cluster = temp_cluster;
			data->file_info[iter_id].file_num_ofs = data->cluster_file_seq;

			if(plist->sum_file_count < ARRAY_SIZE(data->start_file_info))
			{
				data->start_file_info[plist->sum_file_count].file_cluster = temp_cluster;
				data->start_file_info[plist->sum_file_count].file_num_ofs = data->cluster_file_seq;
			}

			plist->sum_file_count++;
#ifdef DYN_PLIST_DEBUG
			SYS_LOG_INF("sum %d id %d, cl%d,blk%lu,seq%d %d\n", plist->sum_file_count, iter_id, temp_cluster,dp->blk_ofs, data->cluster_file_seq, data->dirent->size);
#endif
			if(plist->found_cursor)
			{
				if(plist->next_sum <  PLAY_LIST_NEXT_END_MAX)// next_sum =0
															  // is cur_plist_id
				{
					plist->next_sum ++;//0 is cur play file info
					u16_t update_id = (plist->next_sum + plist->cur_plist_id)% PLAY_LIST_CONTAIN_MAX_CNT;
					plist->file_info[update_id].file_cluster = temp_cluster;
					plist->file_info[update_id].file_num_ofs = data->cluster_file_seq;

#ifdef DYN_PLIST_DEBUG
					printk("next_sum %d id %d cluster %d blk_ofs %d\n", plist->next_sum, update_id, temp_cluster, data->cluster_file_seq);
#endif
				}

			/*set cursor*/
			}else if (cursor && cursor->path &&
				(dp->blk_ofs == cursor_blk_ofs && (u32_t)dp->clust==cursor_cluster&&(uint32_t)(data->dirent->size) == cursor_file_size)) {

				plist->file_seq_num = plist->sum_file_count;

				SYS_LOG_INF("found_cur cur file_seq_num=%d\n", plist->file_seq_num);

				plist->found_cursor = 1;
				plist->cur_plist_id = 0;//cur playlist id start from 0
				plist->next_sum = 0;
				plist->prev_sum = 0;
				plist->file_info[0].file_cluster = temp_cluster;
				plist->file_info[0].file_num_ofs = data->cluster_file_seq;


				if(plist->file_seq_num > 1)
				{
					u16_t cnt = plist->file_seq_num -1;
					for( ; plist->prev_sum < PLAY_LIST_PREV_END_MAX && cnt > 0; cnt-- )
					{
						plist->prev_sum ++;
						iter_id --;

						u16_t plist_pre_id = (PLAY_LIST_CONTAIN_MAX_CNT + plist->cur_plist_id - plist->prev_sum)%PLAY_LIST_CONTAIN_MAX_CNT;
						u16_t iter_pre_id = (ITERATOR_CONTAIN_MAX_CNT + iter_id)% ITERATOR_CONTAIN_MAX_CNT;//seq start from 1, id start from 0

						plist->file_info[plist_pre_id].file_cluster = data->file_info[iter_pre_id].file_cluster;
						plist->file_info[plist_pre_id].file_num_ofs = data->file_info[iter_pre_id].file_num_ofs;

#ifdef DYN_PLIST_DEBUG
						printk("plist_pre_id %d iter_pre_id %d \n", plist_pre_id, iter_pre_id);
						printk("prev_sum %d cluster %d cluster_file_seq %d\n", plist->prev_sum,
								data->file_info[iter_pre_id].file_cluster, data->file_info[iter_pre_id].file_num_ofs);
#endif
					}
				}
			}


			if(!cursor_cluster && !cursor_blk_ofs && !cursor_file_size && !plist->found_cursor)
			{
				plist->file_seq_num = plist->sum_file_count;

				SYS_LOG_INF("no_break_found_cur file_seq_num=%d\n", plist->file_seq_num);

				plist->no_bp_start = 1;
				plist->found_cursor = 1;
				plist->cur_plist_id = 0;//cur playlist id start from 0
				plist->file_info[0].file_cluster = temp_cluster;
				plist->file_info[0].file_num_ofs = data->cluster_file_seq;
			}



			if ((plist->sum_file_count == MAX_SUPPORT_FILE_CNT && !iter_param->max_file_cnt)||
					(plist->sum_file_count == iter_param->max_file_cnt && iter_param->max_file_cnt)) {
				SYS_LOG_WRN("exceed max count %d\n", plist->sum_file_count);
				plist->scaned = 1;
				break;
			}
			continue;
		}

		/* This is a file, file count no change in dir type mode */
		if (data->dirent->type == FS_DIR_ENTRY_FILE && plist->is_file_type == 0) {
			continue;
		}

		/* This is a dir, set had sub folder flag in file type mode */
		if (data->dirent->type == FS_DIR_ENTRY_DIR && plist->is_file_type) {
			plist->has_sub_folder = 1;
			continue;
		}

		if (data->level + 1 >= data->max_level) {
			SYS_LOG_DBG("exceed max level (%d)\n", data->max_level);
			continue;
		}

		if (plist->sum_folder_count + 1 >= CONFIG_PLIST_SUPPORT_FOLDER_CNT) {
			SYS_LOG_INF("exceed max folder(%d)\n", CONFIG_PLIST_SUPPORT_FOLDER_CNT);
			plist->scaned = 1;
			break;
		}

		/* This is directory, down one level if everthing ok */
		res = fs_opendir(data->dirs[data->level + 1], data->full_path);
		if (res) {
			SYS_LOG_WRN("fs_opendir %s failed (res=%d)\n", data->full_path, res);
			continue;
		}

		/*set playlist folder structor*/
		plist->sum_folder_count++;
	#if CONFIG_SUPPORT_FILE_FULL_NAME
		updata_far_cluster_blkofs(plist, data->dirs[data->level], plist->sum_folder_count, data->full_path);
	#endif

		//file_iterator_playlist_set(plist, data->dirs[data->level + 1], plist->sum_folder_count, data->level + 1);
		/*read file first in a new dir*/
		plist->is_file_type = 1;
		/* append a seperator */
		data->full_path[data->full_len] = '/';
		data->full_path[++data->full_len] = 0;
		data->dname_len[++data->level] = data->fname_len + 1;
		data->fname_len = 0;

		/*open dir need to get cluster*/
		dp = &(data->dirs[data->level]->dp);
		temp_cluster = dp->clust;

		SYS_LOG_DBG("down to dir %s\n", data->full_path);

	} while (1);


exit:
	if(plist->scaned)
	{
#ifdef DYN_PLIST_DEBUG
		printk("check0 prev_sum %d next_sum %d, sum %d\n", plist->prev_sum, plist->next_sum, plist->sum_file_count);
#endif

		if(plist->prev_sum < PLAY_LIST_PREV_END_MAX && plist->sum_file_count > (plist->prev_sum + plist->next_sum +1) )
		{
			s16_t iter_id = (plist->sum_file_count -1) % ITERATOR_CONTAIN_MAX_CNT;

			for( ;plist->prev_sum < PLAY_LIST_PREV_END_MAX; )
			{
				//seq start from 1, id start from 0
				plist->prev_sum ++;
				u16_t plist_pre_id = (PLAY_LIST_CONTAIN_MAX_CNT + plist->cur_plist_id - plist->prev_sum)% PLAY_LIST_CONTAIN_MAX_CNT;
				u16_t iter_pre_id = (ITERATOR_CONTAIN_MAX_CNT + iter_id)% ITERATOR_CONTAIN_MAX_CNT;//seq start from 1, id start from 0

				plist->file_info[plist_pre_id].file_cluster = data->file_info[iter_pre_id].file_cluster;
				plist->file_info[plist_pre_id].file_num_ofs = data->file_info[iter_pre_id].file_num_ofs;

#ifdef DYN_PLIST_DEBUG
				printk("prev_sum %d fid%d cluster %d cluster_file_seq %d\n", plist->prev_sum,plist_pre_id,
						data->file_info[iter_pre_id].file_cluster, data->file_info[iter_pre_id].file_num_ofs);
#endif

				iter_id --;

				if((plist->next_sum+plist->prev_sum) >= (plist->sum_file_count -1))
				{
					break;
				}

			}
		}

#ifdef DYN_PLIST_DEBUG
		printk("check1 prev_sum %d next_sum %d, sum %d\n", plist->prev_sum, plist->next_sum, plist->sum_file_count);
#endif
		if(plist->next_sum < PLAY_LIST_NEXT_END_MAX && plist->sum_file_count > (plist->prev_sum + plist->next_sum +1) )
		{
			for(u8_t n = 0; plist->next_sum < PLAY_LIST_NEXT_END_MAX && n < ARRAY_SIZE(data->start_file_info); n++)
			{
				plist->next_sum ++;
				u16_t plist_next_id = (PLAY_LIST_CONTAIN_MAX_CNT + plist->cur_plist_id + plist->next_sum)% PLAY_LIST_CONTAIN_MAX_CNT;
				plist->file_info[plist_next_id].file_cluster = data->start_file_info[n].file_cluster;
				plist->file_info[plist_next_id].file_num_ofs = data->start_file_info[n].file_num_ofs;

#ifdef DYN_PLIST_DEBUG
				printk("next_sum %d ,pid %d, sid %d cluster %d cluster_file_seq %d\n", plist->next_sum,plist_next_id, n,
						plist->file_info[plist->next_sum].file_cluster, plist->file_info[plist->next_sum].file_num_ofs);
#endif
				if((plist->next_sum+plist->prev_sum) >= (plist->sum_file_count -1))
				{
					break;
				}

			}
		}

		SYS_LOG_INF("next_sum %d\n", plist->next_sum);
		SYS_LOG_INF("prev_sum %d\n", plist->prev_sum);

		s32_t i = 0;
		SYS_LOG_INF("data->level %d\n", data->level);
		for (i = data->level; i >= 0; i--)
		{
			SYS_LOG_INF("i %d, dir[%d] 0x%x\n",i, i,(u32_t) data->dirs[i]);
			fs_closedir(data->dirs[i]);
		}

		if(plist->sum_file_count <= PLAY_LIST_CONTAIN_MAX_CNT)
		{

			if (data->dirent) {
				mem_free(data->dirent);
				data->dirent = NULL;
			}

			for (i = 0; i < data->max_level; i++) {
				if (data->dirs[i]) {
					mem_free(data->dirs[i]);
					data->dirs[i] = NULL;
				}
			}

			if (data->dname_len) {
				mem_free(data->dname_len);
				data->dname_len = NULL;
			}
		}


		return data->full_path;
	}
	else
		return NULL;
}

static int file_iterator_update_playlist(struct play_list_t *plist, struct iterator *iter, const void *param)
{
	struct file_iterator_data *data = iter->data;

	int res = -ENOENT;

	if(!plist->inited){
		file_iterator_playlist_init(plist, data, param);

		res = _back_to_topdir(data);
		if (res)
			return res;


		plist->scaned = 0;
		plist->inited = 1;

		plist->is_file_type = 1;
		plist->has_sub_folder = 0;
	}

	return res;
}


static int file_iterator_init(struct iterator *iter, const void *param)
{
	const struct file_iterator_param *iter_param = (struct file_iterator_param *)param;
	struct file_iterator_data *data = NULL;
	uint8_t stat = STA_NODISK;
	int res = -ENOMEM;
	int i = 0;


	if (!iter_param || !iter_param->topdir || iter_param->max_level <= 0)
		return -EINVAL;

	fs_disk_detect(iter_param->topdir, &stat);
	if (stat != STA_DISK_OK) {
		SYS_LOG_ERR("disk not found (%s)\n", iter_param->topdir);
		return -ENODEV;
	}

	data = mem_malloc(sizeof(*data));
	if (!data)
		return -ENOMEM;

	memset(data, 0, sizeof(*data));
	data->match_fn = iter_param->match_fn;
	data->max_level = (uint16_t)iter_param->max_level;
	data->level = -1;

	data->dirent = mem_malloc(sizeof(*data->dirent));
	if (!data->dirent)
		goto err_out;

	for (i = 0; i < data->max_level; i++) {
		data->dirs[i] =  mem_malloc(sizeof(fs_dir_t));
		if (!data->dirs[i])
			goto err_out;
	}

	data->dname_len = mem_malloc(sizeof(*data->dname_len) * data->max_level);
	if (!data->dname_len)
		goto err_out;

	data->full_path = mem_malloc(FULL_PATH_LEN);
	if (!data->full_path)
		goto err_out;

	if (!play_list) {
		play_list = mem_malloc(sizeof(*play_list));
		if (!play_list)
			goto err_out;

	}

	strcpy(data->full_path, iter_param->topdir);
	data->dname_len[0] = strlen(iter_param->topdir);
	data->full_len = data->dname_len[0];
	if (data->full_path[data->full_len - 1] != ':' &&
		data->full_path[data->full_len - 1] != '/') {
		data->full_path[data->full_len++] = '/';
		data->dname_len[0]++;
	}

	data->cursor.path = data->full_path;

	/* set_cursor may access data */
	iter->data = data;

	res = file_iterator_update_playlist(play_list, iter, iter_param);
	if (res)
		goto err_out;

	return 0;

err_out:
	for (i = data->level; i >= 0; i--)
		fs_closedir(data->dirs[i]);

	if (data->dirent) {
		mem_free(data->dirent);
		data->dirent = NULL;
	}

	for (i = 0; i < data->max_level; i++) {
		if (data->dirs[i]) {
			mem_free(data->dirs[i]);
			data->dirs[i] = NULL;
		}
	}

	if (data->dname_len) {
		mem_free(data->dname_len);
		data->dname_len = NULL;
	}

	if (data->full_path) {
		mem_free(data->full_path);
		data->full_path = NULL;
	}
	mem_free(data);

	if (play_list) {
		mem_free(play_list);
		play_list = NULL;
	}

	return res;
}

static const struct iterator_ops file_iterator_ops = {
	.init = file_iterator_init,
	.destroy = file_iterator_destroy,
	.next = file_iterator_next,
	.prev = file_iterator_prev,
	.set_mode = file_iterator_set_mode,
	.get_plist_info = file_iterator_get_plist_info,
	.scan_disk = file_iterator_scan_disk,
	.has_found_cursor = file_iterator_has_found_cursor,
	.need_update_list = file_iterator_need_update_list,
};

struct iterator *file_iterator_create(file_iterator_param_t *param)
{
	return iterator_create((struct iterator_ops *)&file_iterator_ops,param);
}
