﻿/* ***************************************************************************
* file_search.c -- file indexing and searching.
*
* Copyright (C) 2015-2016 by Liu Chao <lc-soft@live.cn>
*
* This file is part of the LC-Finder project, and may only be used, modified, 
* and distributed under the terms of the GPLv2.
*
* By continuing to use, modify, or distribute this file you indicate that you
* have read the license and understand and accept it fully.
*
* The LC-Finder project is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GPL v2 for more details.
*
* You should have received a copy of the GPLv2 along with this file. It is
* usually in the LICENSE.TXT file, If not, see <http://www.gnu.org/licenses/>.
* ****************************************************************************/

/* ****************************************************************************
* file_search.c -- 文件信息的索引与搜索。
*
* 版权所有 (C) 2015-2016 归属于 刘超 <lc-soft@live.cn>
*
* 这个文件是 LC-Finder 项目的一部分，并且只可以根据GPLv2许可协议来使用、更改和
* 发布。
*
* 继续使用、修改或发布本文件，表明您已经阅读并完全理解和接受这个许可协议。
*
* LC-Finder 项目是基于使用目的而加以散布的，但不负任何担保责任，甚至没有适销
* 性或特定用途的隐含担保，详情请参照GPLv2许可协议。
*
* 您应已收到附随于本文件的GPLv2许可协议的副本，它通常在 LICENSE 文件中，如果
* 没有，请查看：<http://www.gnu.org/licenses/>.
* ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_search.h"
#include "sqlite3.h"

#ifdef WIN32
#define strdup _strdup
#endif

static struct DB_Module {
	sqlite3 *db;
	char *sql_buf;
	int sql_buf_len;
} self;

static const char *sql_init = "\
create table if not exists dir (\
	id integer primary key autoincrement,\
	path text not null\
);\
create table if not exists file (\
	id integer primary key autoincrement,\
	did integer not null, \
	path text not null,\
	score integer default 0,\
	create_time int not null,\
	foreign key (did) references dir(id) on delete cascade\
);\
create table if not exists tag (\
	id integer primary key autoincrement,\
	name text\
);\
create table if not exists file_tag_relation (\
	fid integer not null,\
	tid integer not null,\
	foreign key (fid) references file(id) on delete cascade,\
	foreign key (tid) references tag(id) on delete cascade\
);";
static const char *sql_get_dir_list = "\
select id, path from dir order by path asc;\
";
static const char *sql_get_tag_list = "\
select t.id, t.name, count(ftr.tid) from tag t, file_tag_relation ftr \
where ftr.tid = t.id group by ftr.tid order by name asc;\
";
static const char *sql_get_dir_total = "select count(*) from dir;";
static const char *sql_get_tag_total = "select count(*) from tag;";
static const char *sql_add_dir = "insert into dir(path) values(\"%s\");";
static const char *sql_add_tag = "insert into tag(name) values(\"%s\");";
static const char *sql_remove_tag = "delete from tag where id = %d;";
static const char *sql_file_set_score = "update file set score = %d where id = %d;\
";
static const char *sql_file_add_tag = "\
insert into file_tag_relation(fid, did, tid) values(%d, %d, %d);\
";
static const char *sql_file_remove_tag = "\
delete from file_tag_relation where fid = %d and tid = %d;\
";
static const char *sql_add_file = "\
insert into file(did, path, create_time) values(%d, \"%s\", %d);\
";
static const char *sql_get_tag_id = "select id from tag where name = \"%s\";";
static const char *sql_get_dir_id = "select id from dir where path = \"%s\";";
static const char *sql_search_files = "select f.id, f.did, f.score, f.path \
d.path from file f, dir d, file_tag_relation ftr where d.id = f,did";
static const char *sql_count_files = "select count(f.id) from file f, dir d, \
file_tag_relation ftr where d.id = f,did";

/** 缓存 SQL 代码，等到调用 DB_Flush() 时再一次性处理掉 */
static int DB_CacheSQL( const char *sql )
{
	char *buf;
	int len = self.sql_buf_len + strlen(sql) + 2;
	if( self.sql_buf ) {
		buf = realloc( self.sql_buf, sizeof( char )*len );
	} else {
		buf = malloc( sizeof(char)*len );
	}
	if( !buf ) {
		return -1;
	}
	strcat( buf, ";\n" );
	strcat( buf, sql );
	self.sql_buf = buf;
	self.sql_buf_len = len;
	return 0;
}

int DB_Init( void )
{
	int ret;
	char *errmsg;
	printf( "[database] initializing database ...\n" );
	ret = sqlite3_open( "res/data.db", &self.db );
	if( ret != SQLITE_OK ) {
		printf("[database] open failed\n");
		return -1;
	}
	ret = sqlite3_exec( self.db, sql_init, NULL, NULL, &errmsg );
	if( ret != SQLITE_OK ) {
		printf( "[database] error: %s\n", errmsg );
		return -2;
	}
	self.sql_buf = NULL;
	self.sql_buf_len = 0;
	return 0;
}

DB_Dir DB_AddDir( const char *dirpath )
{
	int ret;
	DB_Dir dir;
	sqlite3_stmt *stmt;
	char *errmsg, sql[1024];
	sprintf( sql, sql_add_tag, dirpath );
	ret = sqlite3_exec( self.db, sql, NULL, NULL, &errmsg );
	if( ret != SQLITE_OK ) {
		printf( "[database] error: %s\n", errmsg );
		return NULL;
	}
	sprintf( sql, sql_get_dir_id, dirpath );
	sqlite3_prepare_v2( self.db, sql, -1, &stmt, NULL );
	ret = sqlite3_step( stmt );
	if( ret != SQLITE_ROW ) {
		return NULL;
	}
	dir = malloc( sizeof( DB_DirRec ) );
	dir->id = sqlite3_column_int( stmt, 0 );
	dir->path = strdup( dirpath );
	sqlite3_finalize( stmt );
	return dir;
}

int DB_GetDirs( DB_Dir **outlist )
{
	DB_Dir *list, dir;
	sqlite3_stmt *stmt;
	int ret, i, total = 0;
	sqlite3_prepare_v2( self.db, sql_get_dir_total, -1, &stmt, NULL );
	ret = sqlite3_step( stmt );
	if( ret == SQLITE_ROW ) {
		total = sqlite3_column_int( stmt, 0 );
	}
	sqlite3_finalize( stmt );
	if( total == 0 ) {
		*outlist = NULL;
		return 0;
	}
	list = malloc( sizeof(DB_Dir) * (total + 1) );
	if( !list ) {
		return -1;
	}
	list[total] = NULL;
	sqlite3_prepare_v2( self.db, sql_get_dir_list, -1, &stmt, NULL );
	for( i = 0; i < total; ++i ) {
		ret = sqlite3_step( stmt );
		if( ret != SQLITE_ROW ) {
			list[i] = NULL;
			break;
		}
		dir = malloc( sizeof(DB_DirRec) );
		if( !dir ) {
			list[i] = NULL;
			break;
		}
		dir->id = sqlite3_column_int( stmt, 0 );
		dir->path = strdup( sqlite3_column_text( stmt, 1 ) );
		list[i] = dir;
	}
	sqlite3_finalize( stmt );
	*outlist = list;
	return i;
}

DB_Tag DB_AddTag( const char *tagname )
{
	int ret;
	DB_Tag tag;
	sqlite3_stmt *stmt;
	char *errmsg, sql[1024];
	sprintf( sql, sql_add_tag, tagname );
	ret = sqlite3_exec( self.db, sql, NULL, NULL, &errmsg );
	if( ret != SQLITE_OK ) {
		printf( "[database] error: %s\n", errmsg );
		return NULL;
	}
	sprintf( sql, sql_get_tag_id, tagname );
	sqlite3_prepare_v2( self.db, sql, -1, &stmt, NULL );
	ret = sqlite3_step( stmt );
	if( ret != SQLITE_ROW ) {
		return NULL;
	}
	tag = malloc( sizeof( DB_TagRec ) );
	tag->id = sqlite3_column_int( stmt, 0 );
	tag->name = strdup( tagname );
	tag->count = 0;
	sqlite3_finalize( stmt );
	return tag;
}

void DB_AddFile( DB_Dir dir, const char *filepath, int create_time )
{
	char sql[1024];
	sprintf( sql, sql_add_file, dir->id, filepath, create_time );
	DB_CacheSQL( sql );
}

int DB_GetTags( DB_Tag **outlist )
{
	DB_Tag *list, tag;
	sqlite3_stmt *stmt;
	int ret, i, total = 0;
	sqlite3_prepare_v2( self.db, sql_get_tag_total, -1, &stmt, NULL );
	ret = sqlite3_step( stmt );
	if( ret == SQLITE_ROW ) {
		total = sqlite3_column_int( stmt, 0 );
	}
	sqlite3_finalize( stmt );
	if( total == 0 ) {
		*outlist = NULL;
		return 0;
	}
	list = malloc( sizeof( DB_Dir ) * (total + 1) );
	if( !list ) {
		return -1;
	}
	list[total] = NULL;
	sqlite3_prepare_v2( self.db, sql_get_tag_list, -1, &stmt, NULL );
	for( i = 0; i < total; ++i ) {
		ret = sqlite3_step( stmt );
		if( ret != SQLITE_ROW ) {
			list[i] = NULL;
			break;
		}
		tag = malloc( sizeof( DB_TagRec ) );
		if( !tag ) {
			list[i] = NULL;
			break;
		}
		tag->id = sqlite3_column_int( stmt, 0 );
		tag->name = strdup( sqlite3_column_text( stmt, 1 ) );
		tag->count = sqlite3_column_int( stmt, 2 );
		list[i] = tag;
	}
	sqlite3_finalize( stmt );
	*outlist = list;
	return i;
}

void DBTag_Remove( DB_Tag tag )
{
	char sql[1024];
	sprintf( sql, sql_remove_tag, tag->id );
	DB_CacheSQL( sql );
}

void DBFile_RemoveTag( DB_File file, DB_Tag tag )
{
	char sql[1024];
	sprintf( sql, sql_file_remove_tag, file->id, tag->id );
	DB_CacheSQL( sql );
}

void DBFile_AddTag( DB_File file, DB_Tag tag )
{
	char sql[1024];
	sprintf( sql, sql_file_add_tag, file->id, file->did, tag->id );
	DB_CacheSQL( sql );
}

void DBFile_SetScore( DB_File file, int score )
{
	char sql[1024];
	sprintf( sql, sql_file_set_score, score, file->id );
	DB_CacheSQL( sql );
}

/** 执行文件搜索操作，处理SQL查询结果 */
static int DB_DoSearchFiles( const char *sql, int limit, DB_File **outfiles )
{
	int i = 0;
	DB_File *files;
	sqlite3_stmt *stmt;
	if( !outfiles ) {
		return 0;
	}
	sqlite3_prepare_v2( self.db, sql, -1, &stmt, NULL );
	files = malloc( sizeof( DB_File )*(limit + 1) );
	if( !files ) {
		return -1;
	}
	while( sqlite3_step( stmt ) == SQLITE_ROW && i < limit ) {
		int len, n;
		const char *path, *dirpath;
		DB_File f = malloc( sizeof( DB_FileRec ) );
		f->id = sqlite3_column_int( stmt, 0 );
		f->did = sqlite3_column_int( stmt, 1 );
		f->score = sqlite3_column_int( stmt, 2 );
		path = sqlite3_column_text( stmt, 3 );
		dirpath = sqlite3_column_text( stmt, 4 );
		n = strlen( dirpath );
		len = strlen( f->path ) + n + 2;
		f->path = malloc( len*sizeof( char ) );
		/* 拼接文件夹路径和文件路径 */
		strcpy( f->path, dirpath );
		if( f->path[n - 1] != '/' ) {
			f->path[n] = '/';
			f->path[n + 1] = 0;
		}
		strcat( f->path, path );
		files[i] = f;
		++i;
	}
	sqlite3_finalize( stmt );
	files[limit] = NULL;
	*outfiles = files;
	return i;
}

int DB_SearchFiles( const DB_Query q, DB_File **outfiles, int *total )
{
	int i;
	char buf[256], sql[1024], sql_terms[1024];
	strcpy( sql, sql_search_files );
	if( q->n_tags > 0 && q->tags ) {
		strcat( sql_terms, " and ftr.tid in (" );
		for( i = 0; i < q->n_dirs; ++i ) {
			sprintf( buf, "%d", q->tags[i]->id );
			if( i > 0 ) {
				strcat( sql_terms, ", " );
			}
			strcat( sql_terms, buf );
		}
		strcat( sql_terms, ") and ftr.fid = f.id" );
	}
	if( q->n_dirs > 0 && q->dirs ) {
		strcat( sql_terms, " and f.did in(" );
		for( i = 0; i < q->n_dirs; ++i ) {
			sprintf( buf, "%d", q->dirs[i]->id );
			if( i > 0 ) {
				strcat( sql_terms, ", " );
			}
			strcat( sql_terms, buf );
		}
	}
	if( q->create_time == DESC ) {
		strcat( sql_terms, " order by f.create_time desc" );
	} else if( q->create_time == ASC ) {
		strcat( sql_terms, " order by f.create_time asc" );
	}
	if( q->score != NONE ) {
		if( q->create_time != NONE ) {
			strcat( sql_terms, ", " );
		} else {
			strcat( sql_terms, " order by " );
		}
		if( q->score == DESC ) {
			strcat( sql_terms, "f.score desc" );
		} else {
			strcat( sql_terms, "f.score asc" );
		}
	}
	if( total ) {
		sqlite3_stmt *stmt;
		strcpy( sql, sql_count_files );
		strcat( sql, sql_terms );
		sqlite3_prepare_v2( self.db, sql, -1, &stmt, NULL );
		if( sqlite3_step( stmt ) == SQLITE_ROW ) {
			*total = sqlite3_column_int( stmt, 0 );
		} else {
			*total = 0;
		}
		sqlite3_finalize( stmt );
	}
	sprintf( buf, " limit %d offset %d", q->limit, q->offset );
	strcpy( sql, sql_search_files );
	strcat( sql_terms, buf );
	strcat( sql, sql_terms );
	return DB_DoSearchFiles( sql, q->limit, outfiles );
}

int DB_Flush( void )
{
	int ret;
	char *errmsg, *sql;
	if( !self.sql_buf ) {
		return 0;
	}
	sql = self.sql_buf;
	self.sql_buf = NULL;
	self.sql_buf_len = 0;
	ret = sqlite3_exec( self.db, sql, NULL, NULL, &errmsg );
	if( ret != SQLITE_OK ) {
		printf( "[database] flush error: %s\n", errmsg );
		return -1;
	}
	free( sql );
	return 0;
}