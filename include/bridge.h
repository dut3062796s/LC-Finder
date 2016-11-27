﻿/* ***************************************************************************
 * bridge.h -- a bridge, provides a cross-platform implementation for some
 * interfaces.
 *
 * Copyright (C) 2016 by Liu Chao <lc-soft@live.cn>
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
 * bridge.h -- 桥梁，为某些功能提供跨平台实现。
 *
 * 版权所有 (C) 2016 归属于 刘超 <lc-soft@live.cn>
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

#ifndef LCFINDER_SELECTFOLDER_H
#define LCFINDER_SELECTFOLDER_H

LCFINDER_BEGIN_HEADER

#ifdef _WIN32
#define PLATFORM_WIN32
#if (WINAPI_PARTITION_DESKTOP == 1)
#define PLATFORM_WIN32_DESKTOP
#elif (WINAPI_PARTITION_PC_APP == 1)
#define PLATFORM_WIN32_PC_APP
#endif
//#define PLATFORM_WIN32_DESKTOP_XP
#else
#define PLATFORM_LINUX
#endif

int SelectFolder( char *dirpath, int max_len );

int GetAppDataFolderW( wchar_t *buf, int max_len );

int GetAppInstalledLocationW( wchar_t *buf, int max_len );

void OpenUriW( const wchar_t *uri );

void OpenFileManagerW( const wchar_t *filepath );

int MoveFileToTrashW( const wchar_t *filepath );

int MoveFileToTrash( const char *filepath );

LCFINDER_END_HEADER

#endif
