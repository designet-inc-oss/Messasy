#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <dlfcn.h>
#include <libdgconfig.h>
#include "messasy.h"
#include "msy_config.h"
#include "msy_readmodule.h"
#include "log.h"
#include "so/lib_lm.h"
#include "log.h"

struct modulehandle *mhandle_list = NULL;

/*
 * read_module_config
 *
 * 機能
 *      ファイルをopenし、module設定ファイルを読み込む
 *      エラーはSYSLOG(マクロ)に出力する。
 *
 * 引数
 *      char           *file      ファイル名
 *
 * 返り値
 *      -1             読込権無し、fopen失敗、アロケートエラー。
 *	 0	       正常
 *
 */
int
read_module_config(char *file)
{
    char   line[MAX_CONFIG_LINE + 1];
    FILE   *fp;
    char   *pmsg, *pline, *ptmp = NULL, *head = NULL, *modname = NULL, *modpath = NULL;
    int    nline, status, ret;
    void  *lib_handle;

    /* module設定ファイルの読み込み権検査 */
    if ((pmsg = is_readable_file(file)) != NULL) {
        SYSLOG(LOG_WARNING, pmsg);
        return (-1);
    }

    /* ファイルのオープン */
    fp = fopen(file, "r");
    if (fp == NULL) {
        SYSLOG(LOG_WARNING, ERR_CONF_OPEN, file, strerror(errno));
        return (-1);
    }

    /* 1行ずつ読み込み処理を行う */
    for (nline = 1; fgets(line, MAX_CONFIG_LINE + 1, fp) != NULL; nline++) {

        pline = strchr(line, '\n');
        if (pline == NULL) {
            SYSLOG(LOG_WARNING, ERR_CONF_TOOLONGLINE, file, nline);
            fclose(fp);
            return (-1);
        }
        *pline = '\0';

        if ((line[0] == '#') || (line[0] == '\0')) {
            /* comment or null line */
            continue;
        }

	status = ST_HEAD;
	head = NULL;
	modname = NULL;
	modpath = NULL;
	ptmp = NULL;

        for (pline = line; *pline != '\0'; pline++) {
	    if (ptmp == NULL) {
		ptmp = pline;
	    }
		
	    if (IS_BLANK(*pline)) {
		*pline = '\0';
		if (ptmp == NULL) {
		    SYSLOG(LOG_WARNING, "invalid line(%s: line %d)", file, nline);
		    fclose(fp);
		    return (-1);
		}
	        switch (status) {
		    case  ST_HEAD:
			head = ptmp;
		        status = ST_MODNAME;
		        break;
		    case  ST_MODNAME:
			modname = ptmp;
		        status = ST_PATH;
		        break;
		    case  ST_PATH:
			modpath = ptmp;
		        status = ST_END;
		        break;
		    default:
		        break;
	        }
		ptmp = NULL;
		continue;
	    }

	    if (status == ST_END) {
		SYSLOG(LOG_WARNING, "invalid line(%s: line %d)", file, nline);
		fclose(fp);
		return (-1);
	    }
        }
	if ((status == ST_PATH) && (modpath == NULL)) {
	    modpath = ptmp;
	} else if (status != ST_END) {
	    SYSLOG(LOG_WARNING, "invalid line(%s: line %d)", file, nline);
	    fclose(fp);
	    return (-1);
	}

	if (strcasecmp(head, MODSTR) != 0) {
	    SYSLOG(LOG_WARNING, "invalid line(%s: line %d)", file, nline);
	    fclose(fp);
	    return (-1);
	}

	if (modname == NULL) {
	    SYSLOG(LOG_WARNING, "invalid line(%s: line %d)", file, nline);
	    fclose(fp);
	    return (-1);
	}

        if ((pmsg = is_readable_file(modpath)) != NULL) {
            SYSLOG(LOG_WARNING, pmsg);
	    fclose(fp);
            return (-1);
        }

	/* ライブラリのオープン */
	lib_handle = dlopen(modpath, RTLD_LAZY);
	if (!lib_handle) {
            SYSLOGERROR(ERR_LIB_FILE_OPEN, "read_module_conf", modpath, dlerror());
	    fclose(fp);
            return (-1);
        }

        /* ライブラリポインタのリスト格納 */
        ret = set_lib_handle(modname, lib_handle, &mhandle_list);
        if (ret != 0) {
	    free_lib_handle();
            fclose(fp);
            return (-1);
        }
    }
    fclose(fp);

    return 0;
}

/*
 * free_lib_handle
 *
 * 機能:
 *    dlopenしたモジュールハンドルのリストを開放する
 * 引数:
 *    無し
 * 返値:
 *    無し
 */
void
free_lib_handle()
{
    struct modulehandle *p, *next;

    for (p = mhandle_list; p != NULL; p = next) {
	next = p->mh_next;
	if (p->mh_modulename != NULL) {
	    free(p->mh_modulename);
	}
	dlclose(p->mh_ptr);
	free(p);
    }
}

/*
 * set_lib_handle
 *
 * 機能:
 *    dlopenしたモジュールハンドルのリストを作成する
 * 引数:
 *    char *modname	モジュール名
 *    void *libptr	モジュールハンドル
 *    struct modulehandle **list	モジュールハンドルのリスト
 * 返値:
 *    0		正常
 *    -1	異常
 */
int
set_lib_handle (char *modname, void *libptr, struct modulehandle **list)
{
    struct modulehandle *new_list;

    /* lib_handleポインタを格納する領域の確保 */
    new_list = (struct modulehandle *)malloc(sizeof(struct modulehandle));
    if(new_list == NULL) {
        SYSLOGERROR(ERR_MALLOC, "set_lib_handle", strerror(errno));
        return (-1);
    }
    memset(new_list, 0, sizeof(struct modulehandle));

    new_list->mh_modulename = strdup(modname);
    if(new_list->mh_modulename == NULL) {
        SYSLOGERROR(ERR_MALLOC, "set_lib_handle", strerror(errno));
	free(new_list);
        return (-1);
    }
    new_list->mh_ptr = libptr;
    new_list->mh_next = *list;
    *list = new_list;

    return 0;
}

/*
 * msy_exec_header
 *
 * 機能:
 *    モジュールのヘッダ用関数を実行する関数
 * 引数:
 *    *mP     : Messasyプライベート構造体
 *    *headerf: ヘッダフィールド
 *    *headerv: ヘッダの値
 * 返値:
 *     0: 正常終了
 *    -2: 関数ハンドル作成エラー
 */
int
msy_exec_header(struct mlfiPriv *mP, char *headerf, char *headerv)
{

        /* 返り値判定用 */
    int check = 0;
         /* 関数実行用 */
    int (*func_pointer)(struct mlfiPriv *, char *, char *) = NULL; 
         /* 実行関数名リストをたどる用 */
    struct modulelist *p = NULL; 


    /* header実行関数がある場合実行しつづける */
    for(p = mP->config->cf_exec_header; p != NULL; p = p->mlist_next) {
        func_pointer = (int (*)())(p->mlist_funcptr);
        check = (*func_pointer)(mP, headerf, headerv);
        if (check != 0) {
            SYSLOGERROR(ERR_EXEC_FUNC , "msy_exec_header", p->mlist_funcname);
            return (check);
        }
    }
    return (0);
}

/*
 * msy_exec_body
 *
 * 機能:
 *    モジュールのボディ用関数を実行する関数
 * 引数:
 *    *mP     : Messasyプライベート構造体
 *    *bodyp  : mlfi_bodyが取得したボディの値
 *    bosylen : bodypのサイズ
 * 返値:
 *     0: 正常終了
 *    -2: 関数ハンドル作成エラー
 */
int
msy_exec_body(struct mlfiPriv *mP, u_char *bodyp, size_t bodylen)
{

        /* 返り値判定用 */
    int check = 0;
         /* 関数実行用 */
    int (*func_pointer)(struct mlfiPriv *, u_char *, size_t ) = NULL;
         /* 実行関数名リストをたどる用 */
    struct modulelist *p = NULL; 


    /* body実行関数がある場合実行しつづける */
    for(p = mP->config->cf_exec_body; p != NULL; p = p->mlist_next) {
        func_pointer = (int (*)())p->mlist_funcptr;
        check = (*func_pointer)(mP, bodyp, bodylen);
        if (check != 0) {
            SYSLOGERROR(ERR_EXEC_FUNC , "msy_exec_body", p->mlist_funcname);
            return (check);
        }
    }
    return (0);
}

/*
 * msy_exec_eoh
 *
 * 機能:
 *    モジュールのeoh用関数を実行する関数
 * 引数:
 *    *mP     : Messasyプライベート構造体
 * 返値:
 *     0: 正常終了
 *    -2: 関数ハンドル作成エラー
 */
int
msy_exec_eoh(struct mlfiPriv *mP)
{
        /* 返り値判定用 */
    int check = 0;
         /* 関数実行用 */
    int (*func_pointer)(struct mlfiPriv *);
         /* 実行関数名リストをたどる用 */
    struct modulelist *p = NULL;


    /* eoh実行関数がある場合実行しつづける */
    for(p = mP->config->cf_exec_eoh; p != NULL; p = p->mlist_next) {
        func_pointer = (int (*)())p->mlist_funcptr;
        check = (*func_pointer)(mP);
        if (check != 0) {
            SYSLOGERROR(ERR_EXEC_FUNC , "msy_exec_eoh", p->mlist_funcname);
            return (check);
        }
    }
    return (0);
}

/*
 * msy_exec_eom
 *
 * 機能:
 *    モジュールのeom用関数を実行する関数
 * 引数:
 *    *mP     : Messasyプライベート構造体
 * 返値:
 *     0: 正常終了
 *    -2: 関数ハンドル作成エラー
 */
int
msy_exec_eom(struct mlfiPriv *mP)
{
        /* 返り値判定用 */
    int check = 0;
         /* 関数実行用 */
    int (*func_pointer)(struct mlfiPriv *);
         /* 実行関数名リストをたどる用 */
    struct modulelist *p = NULL; 


    /* eom実行関数がある場合実行しつづける */
    for(p = mP->config->cf_exec_eom; p != NULL; p = p->mlist_next) {
        func_pointer = (int (*)())p->mlist_funcptr;
        check = (*func_pointer)(mP);
        if (check != 0) {
            SYSLOGERROR(ERR_EXEC_FUNC , "msy_exec_eom", p->mlist_funcname);
            return (check);
        }
    }
    return (0);
}

/*
 * msy_exec_abort
 *
 * 機能:
 *    モジュールのabort用関数を実行する関数
 * 引数:
 *    *mP     : Messasyプライベート構造体
 * 返値:
 *     0: 正常終了
 *    -2: 関数ハンドル作成エラー
 */
int
msy_exec_abort(struct mlfiPriv *mP)
{

        /* 返り値判定用 */
    int check = 0;
         /* 関数実行用 */
    int (*func_pointer)(struct mlfiPriv *);
         /* 実行関数名リストをたどる用 */
    struct modulelist *p = NULL; 


    /* abort実行関数がある場合実行しつづける */
    for(p = mP->config->cf_exec_abort; p != NULL; p = p->mlist_next) {
        func_pointer = (int (*)())p->mlist_funcptr;
        check = (*func_pointer)(mP);
        if (check != 0) {
            SYSLOGERROR(ERR_EXEC_FUNC , "msy_exec_abort", p->mlist_funcname);
            return (check);
        }
    }
    return (0);
}
