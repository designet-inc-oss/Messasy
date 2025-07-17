/*
 * messasy
 *
 * Copyright (C) 2006-2024 DesigNET, INC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _LOG_H_
#define _LOG_H_

#define E_STR   strerror(errno)

/*
 * INFO
 */
/* 引数なし */
#define INFO_S_NOSAVEADDRESS    "%08X no address matched"
/* [ From | To ], address */
#define INFO_S_ADDRMATCH        "%08X %s addr matched: %s"
#define INFO_S_ADDRNMATCH       "%08X %s addr not matched: %s"
/* ヘッダフィールド */
#define INFO_S_IGNOREHEADER     "%08X ignored header found: %s"

/*
 * エラー一般
 */
/* 引数なし */
#define ERR_CONFIG_RELOADING "Already reloading config"
/* 関数名, E_STR */
#define ERR_MALLOC        "Cannot allocate memory: %s: %s"
#define ERR_S_MALLOC      "%08X Cannot allocate memory: %s: %s"
#define ERR_THREAD_CREATE "Cannot create thread: %s: %s"
#define ERR_THREAD_DETACH "Cannot detach thread: %s: %s"
/* E_STR */
#define ERR_UNAME       "Cannot get hostname: %s"
#define ERR_S_TIME      "%08X Cannot get current time: %s"
#define ERR_S_LTIME     "%08X Cannot get local time: %s"

/* パス, E_STR */
#define ERR_FOPEN       "Cannot open file: %s: %s"
#define ERR_S_FOPEN     "%08X Cannot open file: %s: %s"
#define ERR_S_FWRITE    "%08X Cannot write file: %s: %s"
#define ERR_S_UNLINK    "%08X Cannot delete file: %s: %s"
#define ERR_S_MKDIR     "%08X Cannot make directory: %s: %s"
#define ERR_S_RMDIR     "%08X Cannot delete directory: %s: %s"
#define ERR_S_MKSTEMP   "%08X Cannot make tempfile: %s: %s"
#define ERR_S_LINK      "%08X Cannot make link: %s: %s"
#define ERR_S_STAT      "%08X Cannot stat path: %s: %s"
#define ERR_S_NDIR      "%08X Not a directory: %s: %s"

/*
 * libdgエラー
 */
/* ライブラリ関数名, E_STR */
#define ERR_LIBFUNC     "%s() failed: %s"
#define ERR_S_LIBFUNC   "%08X %s() failed: %s"

/*
 * libmilterエラー
 */
/* E_STR */
#define ERR_SETCONN     "smfi_setconn() failed: %s"
#define ERR_SETTIMEOUT  "smfi_settimeout() failed: %s"
#define ERR_REGISTER    "smfi_register() failed: %s"
#define ERR_MLFISTART   "smfi_main() failed: %s"

/*
 * msy_configエラー
 */
#define ERR_CONF_ALLOC                  "config memory allocate error"

#define ERR_CONF_TIMEOUT                "invalid timeout value"
#define ERR_CONF_ERRORACTION            "invalid erroraction value"
#define ERR_CONF_SAVEPOLICY             "invalid savepolicy value"

#define ERR_CONF_NULL                   "must be set"
#define ERR_CONF_COMMANDMAXCLIENTS      "invalid commandmaxclients value"
#define ERR_CONF_ADMINPASSWORD          "invalid adminpassword value"
#define ERR_CONF_MYDOMAIN               "invalid mydomain value"
#define ERR_CONF_SAVEMAILADDRESS        "invalid savemailaddress value"

#define ERR_CONF_CONV_ERRORACTION       "erroraction convert error"
#define ERR_CONF_CONV_SAVEPOLICY        "savepolicy convert error"
#define ERR_CONF_CONV_LDAPSCOPE         "ldapscope convert error"
#define ERR_CONF_CONV_SAVEIGNOREHEADER  "saveignoreheader compile error"
#define ERR_CONF_DIV_MYDOMAIN           "mydomain divide error"
#define ERR_CONF_DIV_SAVEMAILADDRESS    "savemailaddress divide error"
#define ERR_CONF_ALLOC_SAVEIGNOREHEADER "regex_memomy allocate error"

#define ERR_CONF_READ                   "config read error"
#define ERR_CONF_CONVERT                "config convert error"
#define ERR_CONF_INITIALIZE             "config initialize error"

#define ERR_CONF_LISTENPORT             "ListenPort: can not use 0 port"
#define ERR_CONF_COMMANDPORT            "CommndPort: can not use 0 port"
#define ERR_CONF_LDAPPORT               "LdapPort: can not use 0 port"

/*
 * filterエラー
 */
#define ERR_LDAP_OPEN                   "ldap open error: Can not connect server"
#define ERR_LDAP_SET                    "ldap set timeout error : %s"
#define ERR_LDAP_BIND                   "ldap bind error : %s"
#define ERR_LDAP_SEARCH                 "ldap search error : %s"
#define ERR_LDAP_FORMAT                 "ldap replace format error"

#define ERR_JUDGE_COMPLEMENT            "address complement error"

#define ERR_JUDGE_MAIL                  "judge mail error"
#define ERR_CHECK_LDAP                  "check ldap error"

/*
 * filter情報
 */
#define JUDGE_LDAP_OUT       "%08X NOT MATCHED (LDAP check): %s"
#define JUDGE_DOMAIN_OUT     "%08X NOT MATCHED (domain check) : %s"
#define JUDGE_ADDRESS_OUT    "%08X NOT MATCHED (address check): %s"
#define JUDGE_CLEARED        "%08X MATCHED: %s"

/*
 * client_sideエラー
 */
#define ERR_READ_SOCK          "[%s] cannot read socket: %s"
#define ERR_READ_SOCKTIMEO     "[%s] cannot read socket: Connection timeout"
#define ERR_SOCK               "Cannot open socket: %s"
#define ERR_BIND               "Cannot bind socket: %s"
#define ERR_LISTEN             "Cannot listen socket: %s"
#define ERR_ACCEPT             "Cannot accept socket: %s"
#define ERR_SELECT             "select: %s"
#define ERR_SETSOCK_KEEP       "Cannot set keepalive option: %s"
#define ERR_SETSOCK_REUSE      "Cannot set reuseaddr option: %s"
#define ERR_SETSOCK_RCVTIMEO   "Cannot set rcvtimeo option: %s"
#define ERR_SETSOCK_SNDTIMEO   "Cannot set sndtimeo option: %s"
#define ERR_HOSTS_CTL          "host %s access denied"
#define ERR_MANY_CONNECT       "Too many connections: %d/%d"

/*
 *追加メッセージ
 */
#define ERR_FILE_EXIST         "nosuch file directory";
#define ERR_FILE_EXECUTE       "execute permission denied";
#define ERR_INVALID_PASSWORD   "invalid password value"
#define ERR_ZIP_COMMAND_TYPE   "invalid enczipcommand value"
#define ERR_TEMPFILE_EXIST     "%08X nosuch tempfile directory: %s: %s"
#define ERR_EXTEND_PART_OPTION_NUM   "invalid EXTEND_PART_OPTION_NUM value: %s"
//#define ERR_SET_ENV            "%08X System space not enought. Cannot set linux environment variable: %s: %s"
#define ERR_SET_ENV            "%08X insufficient space in the environment. Cannot set value to linux environment variable: %s: %s"
#define ERR_EXEC_COMMAND       "%08X cannot execute command: %s: %s"
#define ERR_EXEC_STATUS        "%08X Command execute status: %s: %d"
#define ERR_MEMORY_ALLOC       "cannot allocate memory"
#define ERR_PASSWORD_NULL      "password must be set"
#define ERR_FILE_EXECUTE_PERMITION      "execute permission denied"
#define ERR_ALLOCATE_MEMORY_FORK     "%08X cannot allocate memory for make child process: %s"
#define ERR_S_FORK                   "%08X Cannot duplicate process for run command: %s"
#define ERR_RUN_COMMAND        "%08X Command execute error: %s"
#define ERR_GZIP_CONF          "%08X Cannot read gzip extraconfig"
#define ERR_ENCZIP_CONF          "%08X Cannot read gzip extraconfig"
#endif // _LOG_H_
